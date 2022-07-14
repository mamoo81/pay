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
    if (level == FullyEncrypted && m_segment) m_segment->setEnabled(false);

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

    // create encrypted versions of all the secrets.
    if (m_encryptionLevel == SecretsEncrypted) {
        AES256CBCEncrypt privKeyCrypto(&m_encryptionKey[0], &m_encryptionIR[0], false);

        for (auto i = m_walletSecrets.begin(); i != m_walletSecrets.end(); ++i) {
            auto &secret = i->second;
            assert(secret.privKey.isValid());
            secret.encryptedPrivKey.resize(32);
            privKeyCrypto.encrypt(reinterpret_cast<const char*>(secret.privKey.begin()), 32,
                                  &secret.encryptedPrivKey[0]);
        }

        if (m_hdData) {
            AES256CBCEncrypt crypto(&m_encryptionKey[0], &m_encryptionIR[0], true);
            m_hdData->encryptedWalletMnemonic.resize(m_hdData->walletMnemonic.size() + AES_BLOCKSIZE); // make sure we have enough space
            int newSize = crypto.encrypt(m_hdData->walletMnemonic.data(), m_hdData->walletMnemonic.size(), &m_hdData->encryptedWalletMnemonic[0]);
            m_hdData->encryptedWalletMnemonic.resize(newSize);

            if (!m_hdData->walletMnemonicPwd.empty()) {
                m_hdData->encryptedWalletMnemonicPwd.resize(m_hdData->walletMnemonicPwd.size() + AES_BLOCKSIZE);
                newSize = crypto.encrypt(m_hdData->walletMnemonicPwd.data(), m_hdData->walletMnemonicPwd.size(), &m_hdData->encryptedWalletMnemonicPwd[0]);
                m_hdData->encryptedWalletMnemonicPwd.resize(newSize);
            }
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
        if (m_hdData) {
            // last arg is the padding, which is true here since we don't know the length of the input
            AES256CBCDecrypt crypto(&m_encryptionKey[0], &m_encryptionIR[0], true);
            m_hdData->walletMnemonic.resize(m_hdData->encryptedWalletMnemonic.size());
            int newSize = crypto.decrypt(m_hdData->encryptedWalletMnemonic, &m_hdData->walletMnemonic[0]);
            m_hdData->walletMnemonic.resize(newSize);
            if (!m_hdData->encryptedWalletMnemonicPwd.empty()) {
                m_hdData->walletMnemonicPwd.resize(m_hdData->encryptedWalletMnemonic.size());
                newSize = crypto.decrypt(m_hdData->encryptedWalletMnemonicPwd, &m_hdData->walletMnemonicPwd[0]);
                m_hdData->walletMnemonicPwd.resize(newSize);
            }
            std::string seedWords(m_hdData->walletMnemonic.begin(), m_hdData->walletMnemonic.end());
            std::string pwd(m_hdData->walletMnemonicPwd.begin(), m_hdData->walletMnemonicPwd.end());
            m_hdData->masterKey = HDMasterKey::fromMnemonic(seedWords, pwd);
            assert(m_hdData->masterKey.isValid());
        }

        // that last arg; false is the 'padding' which has to be false since that is the same in the save method.
        AES256CBCDecrypt privKeyDecrypto(&m_encryptionKey[0], &m_encryptionIR[0], false);
        AES256CBCEncrypt privKeyEncryptor(&m_encryptionKey[0], &m_encryptionIR[0], false);
        std::vector<uint8_t> buf(32);
        std::vector<uint32_t> derivationPath;
        for (auto i = m_walletSecrets.begin(); i != m_walletSecrets.end(); ++i) {
            auto &secret = i->second;
            if (secret.encryptedPrivKey.size() == 32) {
                privKeyDecrypto.decrypt(secret.encryptedPrivKey, reinterpret_cast<char*>(&buf[0]));
                secret.privKey.set(buf.begin(), buf.end());
                assert(secret.privKey.isValid());
            }
            else if (m_hdData) {
                // An 'pin to pay' wallet (aka SecretsEncrypted) uses the HDMasterPubkey during sync
                // to generate more private key objects, just without the actual private keys.
                // Now we are unlocked, we can derive it the right way.
                if (derivationPath.empty())
                    derivationPath = m_hdData->derivationPath;
                assert(secret.fromHdWallet);
                derivationPath[derivationPath.size() - 2] = secret.fromChangeChain ? 1 : 0;
                derivationPath[derivationPath.size() - 1] = secret.hdDerivationIndex;
                secret.privKey = m_hdData->masterKey.derive(derivationPath);
                secret.encryptedPrivKey.resize(32);
                privKeyEncryptor.encrypt(reinterpret_cast<const char*>(secret.privKey.begin()), 32,
                                  &secret.encryptedPrivKey[0]);

                m_secretsChanged = true;
            }
            else {
                // then why was the encrypted data missing??
                assert(false);
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
    emit balanceChanged();
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
