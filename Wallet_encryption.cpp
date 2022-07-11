/*
 * This file is part of the Flowee project
 * Copyright (C) 2020-2022 Tom Zander <tom@flowee.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "Wallet.h"
#include "Wallet_p.h"

#include <streaming/BufferPools.h>
#include <streaming/MessageParser.h>
#include <random.h>
#include <crypto/aes.h>
#include <crypto/sha512.h>

#include <QFile>

uint32_t Wallet::encryptionSeed() const
{
    return m_encryptionSeed;
}

void Wallet::setEncryptionSeed(uint32_t seed)
{
    assert(!m_haveEncryptionKey); // Wrong order of calls.
    m_encryptionSeed = seed;
}

bool Wallet::parsePassword(const QString &password)
{
    auto encryptionSeed = m_encryptionSeed;
    const bool firstRun = encryptionSeed == 0;
    if (firstRun)
        GetRandBytes((unsigned char*)&encryptionSeed, sizeof(encryptionSeed));
    const auto bytes = password.toUtf8();

    CSHA512 hasher;
    hasher.write(reinterpret_cast<char*>(&encryptionSeed), sizeof(encryptionSeed));
    hasher.write(bytes.constData(), bytes.size());
    hasher.write(reinterpret_cast<char*>(&encryptionSeed), sizeof(encryptionSeed));
    char buf[CSHA512::OUTPUT_SIZE];
    hasher.finalize(buf);
    for (int i = 0; i < 20000; ++i) {
        hasher.reset().write(buf, sizeof(buf)).finalize(buf);
    }

    uint16_t *crc = reinterpret_cast<uint16_t*>(buf + 48);
    if (!firstRun) { // that means we should check the validity.
        auto data = readSecrets();
        if (m_encryptionLevel == FullyEncrypted) {
            AES256CBCDecrypt crypto(buf, buf + AES256_KEYSIZE, true);
            Streaming::BufferPool pool(data.size());
            int newSize = crypto.decrypt(data.begin(), data.size(), pool.data());
            if (newSize == 0) {
                logCritical() << "Reading (encrypted) secrets file failed";
                return false;
            }
            data = pool.commit(newSize);
        }
        Streaming::MessageParser parser(data);
        if (parser.next() != Streaming::FoundTag)
            return false;
        if (parser.tag() != WalletPriv::CryptoChecksum)
            return false;
        if (parser.longData() != *crc)
            return false; // bad password
    }
    // password is correct, lets update internal wallet state
    m_encryptionSeed = encryptionSeed;
    m_encryptionChecksum = *crc;
    m_encryptionKey.resize(AES256_KEYSIZE);
    m_encryptionIR.resize(AES_BLOCKSIZE);
    memcpy(&m_encryptionKey[0], buf, m_encryptionKey.size());
    memcpy(&m_encryptionIR[0], buf + m_encryptionKey.size(), m_encryptionIR.size());
    memory_cleanse(buf, sizeof(buf));
    m_haveEncryptionKey = true;
    return true;
}

void Wallet::clearEncryptionKey()
{
    m_haveEncryptionKey = false;
    m_encryptionKey.resize(0);
    m_encryptionIR.resize(0);
}

void Wallet::clearDecryptedSecrets()
{
    if (m_encryptionLevel > NotEncrypted) {
        // remove the secrets from our in-memory dataset.
        std::vector<uint8_t> empty;
        for (auto i = m_walletSecrets.begin(); i != m_walletSecrets.end(); ++i) {
            i->second.privKey.set(empty.begin(), empty.end());
            assert(i->second.privKey.isValid() == false);
        }

        if (m_hdData) {
            m_hdData->walletMnemonic.clear();
            m_hdData->walletMnemonicPwd.clear();
            m_hdData->masterKey = HDMasterKey();
        }
    }
    emit encryptionChanged();
}

void Wallet::setEncryption(EncryptionLevel level, const QString &password)
{
    QMutexLocker locker(&m_lock);
    if (level < m_encryptionLevel)
        throw std::runtime_error("Removing encryption from wallet not implemented");
    if (level <= m_encryptionLevel)
        return; // nothing to do

    if (!parsePassword(password)) {
        logCritical() << "Decrypt failed, bad password";
        return;
    }
    assert(m_haveEncryptionKey);

    // the enabled flag is used purely for disabling network sync while the wallet is fully encrypted
    if (m_segment) m_segment->setEnabled(false);

    m_encryptionLevel = level;
    m_secretsChanged = true;
    saveSecrets(); // don't delay as the next step will delete our private keys

    if (level == FullyEncrypted) {
        m_walletChanged = true;
        saveWallet();

        // iterate over all transactions and encrypt+rename those too.
        std::unique_ptr<AES256CBCEncrypt> crypto;
        const QString base = QString::fromStdString(m_basedir.string());
        assert(base.endsWith('/'));
        for (auto i = m_walletTransactions.begin(); i != m_walletTransactions.end(); ++i) {
            QString path = QString::fromStdString(i->second.txid.ToString());
            path.insert(2, '/');

            QFile reader(base + path);
            reader.open(QIODevice::ReadOnly);
            if (!reader.isOpen()) {
                logDebug() << "Missing transaction file";
                continue;
            }
            auto &pool = Streaming::pool(reader.size());
            reader.read(pool.begin(), reader.size());
            reader.close();
            auto orig = pool.commit(reader.size());

            if (crypto.get() == nullptr)
                crypto.reset(new AES256CBCEncrypt(&m_encryptionKey[0], &m_encryptionIR[0], true));
            pool.reserve(orig.size());
            auto newSize = crypto->encrypt(orig.begin(), orig.size(), pool.data());
            assert(newSize > 0);
            auto newFile = pool.commit(newSize);

            uint256 txid(i->second.txid);
            for (int j = 0; j < 32; ++j) {
                txid.begin()[j] += m_encryptionIR[j % m_encryptionIR.size()];
            }
            QString filename = QString::fromStdString(txid.ToString());
            QString localdir = base + filename.left(2);
            boost::system::error_code error;
            boost::filesystem::create_directories(localdir.toStdString(), error);

            const QString newPath(localdir + '/' + filename.mid(2));
            QFile writer(newPath);
            writer.open(QIODevice::WriteOnly);
            if (!writer.isOpen()) {
                logCritical() << "Could not write to" << newPath;
                continue;
            }
            writer.write(newFile.begin(), newFile.size());
            reader.remove();
        }
    }

    clearDecryptedSecrets();
    emit encryptionChanged();
}

Wallet::EncryptionLevel Wallet::encryption() const
{
    return m_encryptionLevel;
}

bool Wallet::decrypt(const QString &password)
{
    QMutexLocker locker(&m_lock);
    assert(m_encryptionLevel > NotEncrypted); // misusage of API.
    if (m_encryptionLevel == NotEncrypted)
        return true;

    // set, and check password correctness
    if (!parsePassword(password)) {
        logCritical() << "Decrypt() failed, bad password";
        return false;
    }
    assert(m_haveEncryptionKey);

    if (m_encryptionLevel == SecretsEncrypted) {
        auto data = readSecrets();
        Streaming::MessageParser parser(data);
        int index = 0;
        // that last arg; false is the 'padding' which has to be false since that is the same in the save method.
        std::unique_ptr<AES256CBCDecrypt> privKeyCrypto(new AES256CBCDecrypt(&m_encryptionKey[0], &m_encryptionIR[0], false));
        // last arg is the padding, which is true here since we don't know the length of the input
        std::unique_ptr<AES256CBCDecrypt> crypto(new AES256CBCDecrypt(&m_encryptionKey[0], &m_encryptionIR[0], true));
        while (parser.next() == Streaming::FoundTag) {
            if (parser.tag() == WalletPriv::Index) {
                index = parser.intData();
            }
            else if (parser.tag() == WalletPriv::PrivKeyEncrypted) {
                if (parser.dataLength() != 32) {
                    logWarning() << "Privkey size invalid. Index:" << index << "size:"
                                  << parser.dataLength();
                    continue;
                }
                auto secret = m_walletSecrets.find(index);
                assert (secret != m_walletSecrets.end());
                std::vector<uint8_t> buf(32);
                auto strData = parser.bytesDataBuffer();
                int newSize = privKeyCrypto->decrypt(strData.begin(), strData.size(), (char*)&buf[0]);
                assert(newSize == 32);
                secret->second.privKey.set(buf.begin(), buf.end());
            }
            else if (parser.tag() == WalletPriv::HDWalletMnemonicEncrypted
                     || parser.tag() == WalletPriv::HDWalletMnemonicPasswordEncrypted) {
                assert(m_hdData);
                auto encrypted = parser.bytesDataBuffer();
                auto &pool = Streaming::pool(encrypted.size());
                const int newSize = crypto->decrypt(encrypted.begin(), encrypted.size(), pool.data());
                auto decrypted = pool.commit(newSize);
                if (parser.tag() == WalletPriv::HDWalletMnemonicEncrypted)
                    m_hdData->walletMnemonic = QString::fromUtf8(decrypted.begin(), decrypted.size());
                else // if (parser.tag() == WalletPriv::HDWalletMnemonicPasswordEncrypted)
                    m_hdData->walletMnemonicPwd = QString::fromUtf8(decrypted.begin(), decrypted.size());
            }
        }
        if (m_hdData && !m_hdData->masterKey.isValid()) {
            m_hdData->masterKey = HDMasterKey::fromMnemonic(m_hdData->walletMnemonic.toStdString(),
                                                            m_hdData->walletMnemonicPwd.toStdString());

            // An 'pin to pay' wallet (aka SecretsEncrypted) uses the HDMasterPubkey during sync
            // to generate more private key objects, just without the actual private keys.
            // Now we are unlocked, lets see if any private keys need to be derived.
            auto derivationPath (m_hdData->derivationPath);
            const auto count = derivationPath.size();
            for (int secretId = m_nextWalletSecretId -1; secretId >= 1; --secretId) {
                auto secretIter = m_walletSecrets.find(secretId);
                assert(secretIter != m_walletSecrets.end());
                auto &secret = secretIter->second;
                assert(secret.fromHdWallet);
                if (secret.privKey.isValid())
                    break;
                derivationPath[count - 2] = secret.fromChangeChain ? 1 : 0;
                derivationPath[count - 1] = secret.hdDerivationIndex;
                secret.privKey = m_hdData->masterKey.derive(derivationPath);
                m_secretsChanged = true;
            }
        }
    }
    else if (m_encryptionLevel == FullyEncrypted) {
        // clear before load, allowing the user to call more than once
        m_walletSecrets.clear();
        m_hdData.reset();
        loadSecrets();
        if (m_walletTransactions.empty())
            loadWallet();

        // the enabled flag is used purely for disabling network sync while the wallet is fully encrypted
        if (m_segment) m_segment->setEnabled(true);
    }
    rebuildBloom();
    recalculateBalance();
    emit encryptionChanged();
    emit paymentRequestsChanged();
    saveSecrets(); // no-op if secrets are unchanged
    return true;
}

bool Wallet::isDecrypted() const
{
    return m_haveEncryptionKey;
}

void Wallet::forgetEncryptedSecrets()
{
    // save*() methods are no-ops if nothing is to be done.
    saveWallet();
    QMutexLocker locker(&m_lock);
    saveSecrets();

    clearEncryptionKey();
    if (m_encryptionLevel == SecretsEncrypted) {
        clearDecryptedSecrets();
        // above method emits encryptionChanged()
    }
    else if (m_encryptionLevel == FullyEncrypted) {
        if (m_segment)
            m_segment->setEnabled(false);
        m_walletSecrets.clear();
        m_walletTransactions.clear();
        m_hdData.reset();
        m_txidCache.clear();
        m_nextWalletTransactionId = 1;
        m_paymentRequests.clear();

        m_unspentOutputs.clear();
        m_lockedOutputs.clear();
        m_txidCache.clear();
        m_balanceConfirmed = 0;
        m_balanceImmature = 0;
        m_balanceUnconfirmed = 0;
        emit encryptionChanged();
        emit balanceChanged();
        emit paymentRequestsChanged();
    }
}
