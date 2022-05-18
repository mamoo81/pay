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
#include "FloweePay.h"
#include "PaymentRequest.h"

#include <NotificationListener.h>
#include <primitives/script.h>
#include <streaming/BufferPool.h>
#include <streaming/BufferPools.h>
#include <streaming/MessageBuilder.h>
#include <streaming/MessageParser.h>
#include <base58.h>
#include <random.h>
#include <crypto/aes.h>
#include <crypto/sha512.h>

#include <QFile>
#include <QSet>
#include <QTimer>
#include <QThread>

// #define DEBUGUTXO

// static
Wallet *Wallet::createWallet(const boost::filesystem::path &basedir, uint16_t segmentId, const QString &name)
{
    Wallet *wallet = new Wallet();
    wallet->m_basedir = basedir / QString("wallet-%1/").arg(segmentId).toStdString();
    wallet->m_segment.reset(new PrivacySegment(segmentId, wallet));
    if (name.isEmpty())
        wallet->m_name = QString("unnamed-%1").arg(segmentId);
    else
        wallet->m_name = name;

    return wallet;
}

Wallet::Wallet()
    : m_lock(QMutex::Recursive),
    m_walletChanged(true),
    m_walletVersion(2)
{
}

Wallet::Wallet(const boost::filesystem::path &basedir, uint16_t segmentId, uint32_t encryptionSeed)
    : m_segment(new PrivacySegment(segmentId, this)),
    m_lock(QMutex::Recursive),
    m_walletVersion(1), // after version 1 we saved the version in the secrets file.
    m_basedir(basedir / QString("wallet-%1/").arg(segmentId).toStdString()),
    m_encryptionSeed(encryptionSeed)
{
    loadSecrets();
    loadWallet();
    rebuildBloom();
    connect (this, SIGNAL(startDelayedSave()), this, SLOT(delayedSave()), Qt::QueuedConnection); // ensure right thread calls us.
}

Wallet::~Wallet()
{
    // these return instantly if nothing has to be saved.
    saveSecrets();
    saveWallet();
}

Wallet::WalletTransaction Wallet::createWalletTransactionFromTx(const Tx &tx, const uint256 &txid, std::map<uint64_t, SignatureType> &types, P2PNet::Notification *notifier) const
{
    if (notifier)
        notifier->txCount++;
    WalletTransaction wtx;
    wtx.txid = txid;

    OutputRef prevTx;
    int inputIndex = -1;
    int outputIndex = -1;
    Output output;
    logDebug() << "new tx." << wtx.txid;
    Tx::Iterator iter(tx);
    while (iter.next() != Tx::End) {
        if (iter.tag() == Tx::PrevTxHash) {
            const uint256 prevTxhash = iter.uint256Data();
            if (++inputIndex == 0)
                wtx.isCoinbase = prevTxhash.IsNull();
            if (!wtx.isCoinbase) {
                auto i = m_txidCache.find(prevTxhash);
                prevTx.setTxIndex((i != m_txidCache.end()) ? i->second : 0);
                if (i != m_txidCache.end())
                    logDebug() << "  Input:" << inputIndex << "prevTx:" << prevTxhash
                               << Log::Hex << i->second << prevTx.encoded();
            }
        } else  if (iter.tag() == Tx::PrevTxIndex) {
            if (prevTx.isValid()) {
                assert(iter.longData() < 0xFFFF); // it would break our scheme
                prevTx.setOutputIndex(iter.intData());
                auto utxo = m_unspentOutputs.find(prevTx.encoded());
                if (utxo != m_unspentOutputs.end()) {
                    // input is spending one of our UTXOs
                    logDebug() << "   -> spent UTXO";
                    wtx.inputToWTX.insert(std::make_pair(inputIndex, prevTx.encoded()));
                    if (notifier)
                        notifier->spent += utxo->second;
                }
                else { // if its not actually an UTXO, forget this ref.
                    prevTx = OutputRef();
                }
            }
        }
        else if (iter.tag() == Tx::TxInScript) {
            if (prevTx.isValid()) {
                // At this point we know that this wallet has created this signature,
                // and that limits the types of output scripts we can expect to essentially
                // p2sh and p2pkh.
                //
                // So we check the signature length, and if its 64 bytes we can be safe
                // to conclude this is a Schnorr based signature.
                CScript script(iter.byteData());
                auto scriptIter = script.begin();
                opcodetype type;
                script.GetOp(scriptIter, type);
                types.insert(std::make_pair(prevTx.encoded(),
                                            type == 65 ? SignedAsSchnorr : SignedAsEcdsa));

                prevTx = OutputRef(); // clear it
            }
        }
        else if (iter.tag() == Tx::OutputValue) {
            ++outputIndex;
            output.value = iter.longData();
        }
        else if (iter.tag() == Tx::OutputScript) {
            output.walletSecretId = findSecretFor(iter.byteData());
            if (output.walletSecretId > 0) {
                logDebug() << "   output"<< outputIndex << "pays to wallet id" << output.walletSecretId;
                wtx.outputs.insert(std::make_pair(outputIndex, output));
                if (notifier)
                    notifier->deposited += output.value;
            }
            else if (iter.dataLength() == 39) {
                // The right length for a CashFusion indicator
                auto outputScript = iter.byteData();
                const char *bytes = outputScript.begin();
                if (bytes[0] == 0x6a // OP_RETURN
                        && bytes[1] == 0x04
                        && bytes[2] == 0x46  // 'F'
                        && bytes[3] == 0x55  // 'U'
                        && bytes[4] == 0x5a  // 'Z'
                        && bytes[5] == 0) {
                    logDebug() << "Transaction" << txid << "is a Cash_Fusion transaction.";
                    wtx.isCashFusionTx = true;
                }
            }
        }
    }
    if (wtx.isCashFusionTx) {
        qint64 sats = 0;
        for (auto o = wtx.outputs.cbegin(); o != wtx.outputs.cend(); ++o) {
            sats += o->second.value;
        }
        wtx.userComment = QString("Fused %1 ➜ %2 (%3 BCH)")
                .arg(wtx.inputToWTX.size())
                .arg(wtx.outputs.size())
                .arg(FloweePay::priceToString(sats, FloweePay::BCH));
    }
    return wtx;
}

// check if we need to create more private keys based on if this transaction
// used private keys close to the index we have created and keep track off.
bool Wallet::updateHDSignatures(const Wallet::WalletTransaction &wtx, bool &updateBloom)
{
    if (m_hdData.get() == nullptr)
        return false;
    QMutexLocker locker(&m_lock);

    static constexpr int ExtraAddresses = 50;
    int needChangeAddresses = 0;
    int needMainAddresses = 0;

    for (auto i = wtx.outputs.begin(); i != wtx.outputs.end(); ++i) {
        auto privKey = m_walletSecrets.find(i->second.walletSecretId);
        assert(privKey != m_walletSecrets.end()); // invalid wtx
        const WalletSecret &ws = privKey->second;
        if (ws.fromHdWallet) {
            if (ws.fromChangeChain) {
                assert(m_hdData->lastChangeKey >= ws.hdDerivationIndex);
                if (m_hdData->lastChangeKey - ws.hdDerivationIndex < 15)
                    needChangeAddresses = ExtraAddresses;
                updateBloom |= m_hdData->lastIncludedChangeKey - ws.hdDerivationIndex < 10;
            } else {
                assert(m_hdData->lastMainKey >= ws.hdDerivationIndex);
                if (m_hdData->lastMainKey - ws.hdDerivationIndex < 15)
                    needMainAddresses = ExtraAddresses;
                updateBloom |= m_hdData->lastIncludedMainKey - ws.hdDerivationIndex < 5;
            }
        }
    }

    if (needChangeAddresses + needMainAddresses > 0) {
        deriveHDKeys(needMainAddresses, needChangeAddresses);
        return true;
    }
    return false;
}

void Wallet::deriveHDKeys(int mainChain, int changeChain, uint32_t startHeight)
{
    // mutex already locked
    while (changeChain + mainChain > 0) {
        m_secretsChanged = true;
        WalletSecret secret;
        secret.initialHeight = startHeight;
        secret.fromHdWallet = true;
        const auto count = m_hdData->derivationPath.size();
        assert(count >= 2);
        if (mainChain > 0) {
            --mainChain;
            secret.hdDerivationIndex = ++m_hdData->lastMainKey;
            m_hdData->derivationPath[count - 2] = 0;
            logDebug() << "Creating new private key. Derivation: 0," << secret.hdDerivationIndex;
        } else {
            assert(changeChain > 0);
            --changeChain;
            secret.fromChangeChain = true;
            secret.hdDerivationIndex = ++m_hdData->lastChangeKey;
            logDebug() << "Creating new private key for change chain:" << secret.hdDerivationIndex;
            m_hdData->derivationPath[count - 2] = 1;
        }
        m_hdData->derivationPath[count - 1] = secret.hdDerivationIndex;
        secret.privKey = m_hdData->masterKey.derive(m_hdData->derivationPath);

        const CPubKey pubkey = secret.privKey.getPubKey();
        secret.address = pubkey.getKeyId();
        m_walletSecrets.insert(std::make_pair(m_nextWalletSecretId++, secret));
    }

    rebuildBloom();
    saveSecrets(); // no-op if secrets are unchanged
}

void Wallet::updateSignatureTypes(const std::map<uint64_t, SignatureType> &txData)
{
    // this basically processes the output argument received from createWalletTransactionFromTx
    for (auto i = txData.begin(); i != txData.end(); ++i) {
        OutputRef ref(i->first);
        auto iter = m_walletTransactions.find(ref.txIndex());
        assert(iter != m_walletTransactions.end());
        auto outIter = iter->second.outputs.find(ref.outputIndex());
        assert(outIter != iter->second.outputs.end());
        auto secretIter = m_walletSecrets.find(outIter->second.walletSecretId);
        assert(secretIter != m_walletSecrets.end());
        auto &secret = secretIter->second;
        if (secret.signatureType == NotUsedYet) {
            secret.signatureType = i->second;
        }
    }
}

void Wallet::newTransaction(const Tx &tx)
{
    int firstNewTransaction;
    P2PNet::Notification notification;
    notification.privacySegment = int(m_segment->segmentId());
    bool createdNewKeys = false;
    {
        QMutexLocker locker(&m_lock);
        firstNewTransaction = m_nextWalletTransactionId;
        const uint256 txid = tx.createHash();
        if (m_txidCache.find(txid) != m_txidCache.end()) // already known
            return;

        std::map<uint64_t, SignatureType> signatureTypes;
        WalletTransaction wtx = createWalletTransactionFromTx(tx, txid, signatureTypes, &notification);
        Q_ASSERT(wtx.isCoinbase == false);
        if (wtx.outputs.empty() && wtx.inputToWTX.empty()) {
            // no connection to our UTXOs
            if (--m_bloomScore < 25)
                rebuildBloom();
            return;
        }
        setUserOwnedWallet(true);
        wtx.minedBlockHeight = WalletPriv::Unconfirmed;
        bool dummy = false;
        while (updateHDSignatures(wtx, dummy)) {
            // if we added a bunch of new private keys, then rerun the matching
            // so we make sure we matched all we can
            auto newWtx = createWalletTransactionFromTx(tx, txid, signatureTypes, &notification);
            wtx.outputs = newWtx.outputs;
            createdNewKeys = true;
        }

        // Remember the signature type used for specific private keys
        updateSignatureTypes(signatureTypes);

        // Mark UTXOs locked that this tx spent to avoid double spending them.
        for (auto i = wtx.inputToWTX.begin(); i != wtx.inputToWTX.end(); ++i) {
            m_lockedOutputs.insert(std::make_pair(i->second, m_nextWalletTransactionId));
        }

        // insert new UTXOs and update possible hits in the paymentRequests
        for (auto i = wtx.outputs.begin(); i != wtx.outputs.end(); ++i) {
            uint64_t key = m_nextWalletTransactionId;
            key <<= 16;
            key += i->first;
            logDebug() << "   inserting output"<< i->first << Log::Hex << "TxIndex:" << i->second.walletSecretId << "outRef:" << key;
            m_unspentOutputs.insert(std::make_pair(key, i->second.value));

            const int privKeyId = i->second.walletSecretId;
            for (auto pr : qAsConst(m_paymentRequests)) {
                if (pr->m_privKeyId == privKeyId) {
                    pr->addPayment(key, i->second.value);
                    wtx.userComment = pr->message();
                }
            }
        }

        // and remember the transaction
        m_txidCache.insert(std::make_pair(wtx.txid, m_nextWalletTransactionId));
        m_walletTransactions.insert(std::make_pair(m_nextWalletTransactionId++, wtx));
        m_walletChanged = true;

        logCritical() << "Wallet" << m_segment->segmentId() << "claims" << tx.createHash() << "[unconfirmed]";
    } // mutex scope
    saveTransaction(tx);
    recalculateBalance();
    emit startDelayedSave();

    emit utxosChanged();
    emit appendedTransactions(firstNewTransaction, 1);
#ifndef IN_TESTS // don't call singleton in unit tests
    FloweePay::instance()->p2pNet()->notifications().notifyNewTransaction(notification);
#endif
    if (createdNewKeys)
        rebuildBloom();
}

void Wallet::newTransactions(const BlockHeader &header, int blockHeight, const std::deque<Tx> &blockTransactions)
{
    auto transactions = WalletPriv::sortTransactions(blockTransactions);
    std::deque<Tx> transactionsToSave;
    std::set<int> ejectedTransactions;
    int firstNewTransaction;
    bool needNewBloom = false;
    {
        QMutexLocker locker(&m_lock);
        firstNewTransaction = m_nextWalletTransactionId;
        setUserOwnedWallet(true);
        for (auto &tx: transactions) {
            const uint256 txid = tx.createHash();
            WalletTransaction wtx;

            auto oldTx = m_txidCache.find(txid);
            int walletTransactionId = m_nextWalletTransactionId;
            P2PNet::Notification notification;
            notification.privacySegment = int(m_segment->segmentId());
            std::map<uint64_t, SignatureType> signatureTypes;
            if (oldTx == m_txidCache.end()) {
                wtx = createWalletTransactionFromTx(tx, txid, signatureTypes, &notification);
                notification.blockHeight = blockHeight;
                if (wtx.outputs.empty() && wtx.inputToWTX.empty()) {
                    // no connection to our UTXOs
                    if (--m_bloomScore < 25)
                        needNewBloom = true;
                    continue;
                }
            }
            else {
                // we already seen it before.
                wtx = m_walletTransactions.find(oldTx->second)->second;
                // check if the one we saw was unconfirmed or not.
                if (wtx.minedBlockHeight >= 0)
                    continue;
                walletTransactionId = oldTx->second;
            }
            const bool wasUnconfirmed = wtx.minedBlockHeight == WalletPriv::Unconfirmed;
            while (updateHDSignatures(wtx, needNewBloom)) {
                // if we added a bunch of new private keys, then rerun the matching
                // so we make sure we matched all we can
                auto newWtx = createWalletTransactionFromTx(tx, txid, signatureTypes, &notification);
                wtx.outputs = newWtx.outputs;
                needNewBloom = true;
            }
            wtx.minedBlock = header.createHash();
            wtx.minedBlockHeight = blockHeight;

            // Remember the signature type used for specific private keys
            updateSignatureTypes(signatureTypes);

            // remove UTXOs this Tx spent
            for (auto i = wtx.inputToWTX.begin(); i != wtx.inputToWTX.end(); ++i) {
                auto iter = m_unspentOutputs.find(i->second);
                assert(iter != m_unspentOutputs.end()); // a double spend? With checked merkle-block? Thats...odd.
                if (iter != m_unspentOutputs.end())
                    m_unspentOutputs.erase(iter);

                // unlock UTXOs
                auto lockedIter = m_lockedOutputs.find(i->second);
                if (lockedIter != m_lockedOutputs.end()) {
                    if (lockedIter->second  > 0 && lockedIter->second != walletTransactionId) {
                        // if this output was locked by another transaction then that means
                        // that other transaction was rejected (double spent) since the
                        // tx we are now processing made it into a block.
                        // remember so we can process ejected transactions below.
                        ejectedTransactions.insert(lockedIter->second);
                    }
                    m_lockedOutputs.erase(lockedIter);
                }
            }

            // process new UTXOs
            for (auto i = wtx.outputs.begin(); i != wtx.outputs.end(); ++i) {
                auto key = OutputRef(m_nextWalletTransactionId, i->first).encoded();
                if (!wasUnconfirmed) { // unconfirmed transactions already had their outputs added
                    logDebug() << "   inserting output"<< i->first << Log::Hex << i->second.walletSecretId << key;
                    m_unspentOutputs.insert(std::make_pair(key, i->second.value));
                    if (!m_singleAddressWallet && i->second.value == 547) {
                        // special case so-called 'spam' outputs and instantly lock it for privacy reasons
                        // those outputs can only be spent by combining them with others, combining inputs
                        // is a potential loss of privacy (makes keys tracable) that is not worth the benefit
                        // of this low value output.
                        m_lockedOutputs.insert(std::make_pair(key, -1)); // -1 is our reason
                    }
                }

                // check the payment requests
                const int privKeyId = i->second.walletSecretId;
                auto ws = m_walletSecrets.find(privKeyId);
                if (ws != m_walletSecrets.end() && ws->second.initialHeight == 0) {
                    ws->second.initialHeight = blockHeight;
                    needNewBloom = true; // make sure we let the remote know about our 'gap' addresses
                }

                for (auto pr : qAsConst(m_paymentRequests)) {
                    if (pr->m_privKeyId == privKeyId) {
                        pr->addPayment(key, i->second.value, blockHeight);
                        wtx.userComment = pr->message();
                    }
                }
            }


            // and remember the transaction
            if (oldTx == m_txidCache.end()) {
                Q_ASSERT(walletTransactionId == m_nextWalletTransactionId);
                m_txidCache.insert(std::make_pair(wtx.txid, m_nextWalletTransactionId));
                m_walletTransactions.insert(std::make_pair(m_nextWalletTransactionId++, wtx));
                transactionsToSave.push_back(tx);
            }
            else { // update the old one with the new data.
                auto wtxIter = m_walletTransactions.find(oldTx->second);
                assert(wtxIter != m_walletTransactions.end());
                wtxIter->second = wtx;
            }
            m_walletChanged = true;

            logCritical() << "Wallet" << m_segment->segmentId() << "claims" << tx.createHash() << "@" << blockHeight;
            if (wasUnconfirmed)
                emit transactionConfirmed(walletTransactionId);
            if (notification.blockHeight > 0) {
#ifndef IN_TESTS // don't call singleton in unit tests
                FloweePay::instance()->p2pNet()->notifications().notifyNewTransaction(notification);
#endif
            }
        }
        assert(m_nextWalletTransactionId - firstNewTransaction == int(transactionsToSave.size()));

        // In processing the transactions from a block we might find that one of our
        // unconfirmed transactions ended up being rejected due to double spend.
        // We need to update our internal book keeping and eject the transaction.
        // We actually mark it Rejected (in the blockHeight).
        for (auto ejectedTx : ejectedTransactions) {
            auto tx = m_walletTransactions.find(ejectedTx);
            Q_ASSERT(tx != m_walletTransactions.end());
            logDebug() << "Confirmed transaction(s) in block" << blockHeight <<
                          "made invalid transaction:" << ejectedTx << tx->second.txid;
            auto &wtx = tx->second;
            wtx.minedBlockHeight = WalletPriv::Rejected;
            // Any outputs we locked need to be unlocked
            for (auto i = wtx.inputToWTX.begin(); i != wtx.inputToWTX.end(); ++i) {
                auto iter = m_lockedOutputs.find(i->second);
                if (iter != m_lockedOutputs.end() && iter->second == ejectedTx)
                    m_lockedOutputs.erase(iter);
            }
            // Any UTXOs we created for this rejected Tx need to be removed
            for (auto i = wtx.outputs.begin(); i != wtx.outputs.end(); ++i) {
                uint64_t key = ejectedTx;
                key <<= 16;
                key += i->first;
                auto utxo = m_unspentOutputs.find(key);
                if (utxo != m_unspentOutputs.end())
                    m_unspentOutputs.erase(utxo);

                // check the payment requests
                const int privKeyId = i->second.walletSecretId;
                for (auto pr : qAsConst(m_paymentRequests)) {
                    if (pr->m_privKeyId == privKeyId)
                        pr->paymentRejected(key, i->second.value);
                }
            }
        }
    } // mutex scope

    if (!transactionsToSave.empty()) {
        emit utxosChanged();
        emit appendedTransactions(firstNewTransaction, transactionsToSave.size());
        for (auto &tx : transactionsToSave) { // save the Tx to disk.
            saveTransaction(tx);
        }
        emit startDelayedSave();
    }
    recalculateBalance();
    if (needNewBloom)
        rebuildBloom();
}

void Wallet::saveTransaction(const Tx &tx)
{
    QString dir("%1/%2/");
    dir = dir.arg(QString::fromStdString(m_basedir.string()));
    try {
        uint256 txid(tx.createHash());
        auto data = tx.data();
        if (m_encryptionLevel == FullyEncrypted) {
            // we encrypt transactions on-disk. Also we scramble their filename
            if (!m_haveEncryptionKey)
                throw std::runtime_error("No encryption password set");
            for (int i = 0; i < 32; ++i) {
                txid.begin()[i] += m_encryptionIR[i % m_encryptionIR.size()];
            }

            AES256CBCEncrypt crypto(&m_encryptionKey[0], &m_encryptionIR[0], true);
            auto &pool = Streaming::pool(data.size() + AES_BLOCKSIZE);
            int size = crypto.encrypt(data.begin(), data.size(), pool.data());
            data = pool.commit(size);
        }
        QString filename = QString::fromStdString(txid.ToString());
        QString localdir = dir.arg(filename.left(2));
        boost::system::error_code error;
        boost::filesystem::create_directories(localdir.toStdString(), error);
        filename = filename.mid(2);

        std::ofstream txSaver;
        txSaver.open((localdir + filename).toStdString());
        txSaver.write(data.begin(), data.size());
    } catch (const std::exception &e) {
        logFatal() << "Could not store transaction" << e.what();
        throw;
    }
}

Tx Wallet::loadTransaction(const uint256 &txid, Streaming::BufferPool &pool) const
{
    uint256 filename(txid);
    if (m_encryptionLevel == FullyEncrypted) {
        if (!m_haveEncryptionKey)
            throw std::runtime_error("No encryption password set");
        for (int i = 0; i < 32; ++i) {
            filename.begin()[i] += m_encryptionIR[i % m_encryptionIR.size()];
        }
    }
    QString path = QString::fromStdString(filename.ToString());
    path.insert(2, '/');
    path = QString::fromStdString(m_basedir.string()) + path;

    QFile reader(path);
    if (reader.open(QIODevice::ReadOnly)) {
        int txSize = reader.size();
        pool.reserve(txSize);
        reader.read(pool.begin(), txSize);
        if (m_encryptionLevel == FullyEncrypted) {
            // decrypt the tx
            assert(m_haveEncryptionKey); // checked above
            AES256CBCDecrypt crypto(&m_encryptionKey[0], &m_encryptionIR[0], true);
            auto encryptedTx = pool.commit(txSize);
            pool.reserve(txSize);
            txSize = crypto.decrypt(encryptedTx.begin(), txSize, pool.data());
        }
        return Tx(pool.commit(txSize));
    }
    // return empty tx
    return Tx();
}

bool Wallet::userOwnedWallet() const
{
    return m_userOwnedWallet;
}

void Wallet::setUserOwnedWallet(bool userOwnedWallet)
{
    if (m_userOwnedWallet == userOwnedWallet)
        return;
    m_userOwnedWallet = userOwnedWallet;
    if (m_userOwnedWallet && m_segment->priority() == PrivacySegment::Last) // user-owned changed from false to true
        m_segment->setPriority(PrivacySegment::Normal);
    m_secretsChanged = true;
    emit userOwnedChanged();
}

void Wallet::setTransactionComment(const Tx &transaction, const QString &comment)
{
    QMutexLocker locker(&m_lock);
    auto i = m_txidCache.find(transaction.createHash());
    if (i != m_txidCache.end()) {
        auto wtxIter = m_walletTransactions.find(i->second);
        assert(wtxIter != m_walletTransactions.end());
        auto &wtx = wtxIter->second;
        if (wtx.userComment != comment) {
            wtx.userComment = comment;
            m_walletChanged = true;

            emit transactionChanged(wtxIter->first);
            emit startDelayedSave();
        }
    }
}

std::map<int, Wallet::WalletSecret> Wallet::walletSecrets() const
{
    QMutexLocker locker(&m_lock);
    return m_walletSecrets;
}

Wallet::KeyDetails Wallet::fetchKeyDetails(int privKeyId) const
{
    KeyDetails details = { 0, 0, 0 };
    QMutexLocker locker(&m_lock);
    for (const auto &row : m_walletTransactions) {
        const WalletTransaction &wt = row.second;
        for (auto i = wt.outputs.begin(); i != wt.outputs.end(); ++i) {
            if (i->second.walletSecretId == privKeyId) {
                ++details.historicalCoins;
                OutputRef ref(row.first, i->first);
                auto utxo = m_unspentOutputs.find(ref.encoded());
                if (utxo != m_unspentOutputs.end()) {
                    details.saldo += utxo->second;
                    ++details.coins;
                }
            }
        }
    }

    return details;
}

int Wallet::findPrivKeyId(const CKeyID &address) const
{
    for (const auto &priv : m_walletSecrets) {
        if (priv.second.address == address) {
            return priv.first;
        }
    }
    return -1;
}

bool Wallet::isHDWallet() const
{
    return m_hdData.get() != nullptr;
}

QString Wallet::hdWalletMnemonic() const
{
    if (m_hdData.get())
        return m_hdData->walletMnemonic;
    return QString();
}

QString Wallet::hdWalletMnemonicPwd() const
{
    if (m_hdData.get())
        return m_hdData->walletMnemonicPwd;
    return QString();
}

QString Wallet::derivationPath() const
{
    if (m_hdData.get()) {
        QString answer("m");
        assert(m_hdData->derivationPath.size() >= 2); //
        for (size_t i = 0; i < m_hdData->derivationPath.size() - 2; ++i) {
            uint32_t value = m_hdData->derivationPath.at(i);
            const bool hardened = value & HDMasterKey::Hardened;
            answer += QString("/%1").arg(value & 0x7FFFFFFF);
            if (hardened)
                answer += '\'';
        }
        return answer;
    }
    return QString();
}

void Wallet::createHDMasterKey(const QString &mnemonic, const QString &pwd, const std::vector<uint32_t> &derivationPath, uint32_t startHeight)
{
    assert(m_hdData.get() == nullptr);
    if (m_hdData.get()) {
        logFatal() << "Refusing to replace HDMasterkey with new one";
        return;
    }
    // we only convert here in order to be 100% certain that the input from the user is
    // encoded properly, regardless of the underlying OS settings.

    // Explicitly use utf8 encoding.
    std::string m(mnemonic.toUtf8().constData());
    std::string p(pwd.toUtf8().constData());
    m_hdData.reset(new HierarchicallyDeterministicWalletData(m, p));
    m_hdData->derivationPath = derivationPath;
    // append two random numbers, to make clear the full length
    m_hdData->derivationPath.push_back(0);
    m_hdData->derivationPath.push_back(0);

    // Wallets add to the derivation path 2 items, the first is typically used
    // to indicate 0=receive-keys, 1=change-keys
    // So to reuse the derivationPath we store the last-used-index elsewhere.
    m_hdData->lastMainKey = -1;
    m_hdData->lastChangeKey = -1;
    QMutexLocker locker(&m_lock);
    deriveHDKeys(50, 50, startHeight);
    saveSecrets();
}

int Wallet::lastTransactionTimestamp() const
{
    QMutexLocker locker(&m_lock);
    for (auto it = m_walletTransactions.crbegin(); it != m_walletTransactions.crend(); ++it) {
        if (it->second.minedBlockHeight > 1)
            return it->second.minedBlockHeight;
    }
    return 0;
}

bool Wallet::lockUTXO(OutputRef outputRef)
{
    QMutexLocker locker(&m_lock);
    const auto ref = outputRef.encoded();
    auto utxoLink = m_unspentOutputs.find(ref);
    if (utxoLink == m_unspentOutputs.end())
        return false;
    if (m_lockedOutputs.find(ref) != m_lockedOutputs.end())
        return false;
    m_lockedOutputs.insert(std::make_pair(ref, 0));
    return true;
}

bool Wallet::unlockUTXO(OutputRef outputRef)
{
    QMutexLocker locker(&m_lock);
    const auto ref = outputRef.encoded();
    const auto i = m_lockedOutputs.find(ref);
    if (i == m_lockedOutputs.end())
        return false;
    if (i->second > 0) // only allow manual unlocking manually locked outputs
        return false;
    m_lockedOutputs.erase(i);
    return true;
}

bool Wallet::isLocked(OutputRef outputRef) const
{
    QMutexLocker locker(&m_lock);
    return m_lockedOutputs.find(outputRef.encoded()) != m_lockedOutputs.end();
}

void Wallet::performUpgrades()
{
    if (m_walletVersion == 1 && !m_walletTransactions.empty() && !m_walletSecrets.empty()) {
        // this wallet needs to rescan its existing transactions since older clients
        // didn't populate that field yet on receiving transactions
        populateSigType();
        m_walletVersion = 2;
        m_secretsChanged = true;
        delayedSave();
    }
}

QList<PaymentRequest *> Wallet::paymentRequests() const
{
    QMutexLocker locker(&m_lock);
    return m_paymentRequests;
}

uint32_t Wallet::encryptionSeed() const
{
    return m_encryptionSeed;
}

void Wallet::setEncryptionSeed(uint32_t seed)
{
    assert(!m_haveEncryptionKey); // Wrong order of calls.
    m_encryptionSeed = seed;
}

void Wallet::setEncryptionPassword(const QString &password)
{
    if (m_encryptionSeed == 0)
        GetRandBytes((unsigned char*)&m_encryptionSeed, sizeof(m_encryptionSeed));

    const auto bytes = password.toUtf8();

    CSHA512 hasher;
    hasher.write(reinterpret_cast<char*>(&m_encryptionSeed), sizeof(m_encryptionSeed));
    hasher.write(bytes.constData(), bytes.size());
    hasher.write(reinterpret_cast<char*>(&m_encryptionSeed), sizeof(m_encryptionSeed));
    char buf[CSHA512::OUTPUT_SIZE];
    hasher.finalize(buf);
    for (int i = 0; i < 20000; ++i) {
        hasher.reset().write(buf, sizeof(buf)).finalize(buf);
    }

    m_encryptionKey.resize(AES256_KEYSIZE);
    m_encryptionIR.resize(AES_BLOCKSIZE);
    memcpy(&m_encryptionKey[0], buf, m_encryptionKey.size());
    memcpy(&m_encryptionIR[0], buf + m_encryptionKey.size(), m_encryptionIR.size());
    memory_cleanse(buf, sizeof(buf));
    m_haveEncryptionKey = true;
}

bool Wallet::hasEncryptionPassword() const
{
    return m_haveEncryptionKey;
}

void Wallet::clearEncryptionPassword()
{
    QMutexLocker locker(&m_lock);
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

        // TODO implement cleaning for HD wallets too
        assert(m_hdData.get() == nullptr);
    }
    emit encryptionChanged();
}

void Wallet::setEncryption(EncryptionLevel level)
{
    QMutexLocker locker(&m_lock);
    if (level < m_encryptionLevel)
        throw std::runtime_error("Removing encryption from wallet not implemented");

    if (level > m_encryptionLevel) {
        if (!m_haveEncryptionKey)
            throw std::runtime_error("Set encryption password first!");
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
                for (int i = 0; i < 32; ++i) {
                    txid.begin()[i] += m_encryptionIR[i % m_encryptionIR.size()];
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
    }

    clearDecryptedSecrets();
}

Wallet::EncryptionLevel Wallet::encryption() const
{
    return m_encryptionLevel;
}

void Wallet::decrypt()
{
    QMutexLocker locker(&m_lock);
    assert(m_encryptionLevel > NotEncrypted); // misusage.
    if (!m_haveEncryptionKey)
        throw std::runtime_error("Need password set first");

    if (m_encryptionLevel == SecretsEncrypted) {
        auto data = readSecrets();
        Streaming::MessageParser parser(data);
        int index = 0;
        std::unique_ptr<AES256Decrypt> crypto;
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
                if (crypto.get() == nullptr) {
                    crypto.reset(new AES256Decrypt(&m_encryptionKey[0]));
                    m_encryptionLevel = SecretsEncrypted;
                }
                std::vector<uint8_t> buf(32);
                assert(buf.size() == 32);
                auto data = parser.bytesDataBuffer();
                crypto->decrypt(reinterpret_cast<char*>(&buf[0]), data.begin());
                crypto->decrypt(reinterpret_cast<char*>(&buf[16]), data.begin() + 16);
                secret->second.privKey.set(buf.begin(), buf.end());
            }
        }
        // TODO read HD wallet keys
    }
    else if (m_encryptionLevel == FullyEncrypted) {
        // clear before load, allowing the user to call
        // clearDecryptedSecrets() and then refresh from disk the secrets.
        m_walletSecrets.clear();
        loadSecrets();
        if (m_walletTransactions.empty())
            loadWallet();
    }
    emit encryptionChanged();
}

void Wallet::addPaymentRequest(PaymentRequest *pr)
{
    QMutexLocker locker(&m_lock);
    m_paymentRequests.append(pr);
    m_walletChanged = true;
    emit paymentRequestsChanged();
}

void Wallet::removePaymentRequest(PaymentRequest *pr)
{
    QMutexLocker locker(&m_lock);
    m_paymentRequests.removeAll(pr);
    m_walletChanged = true;
    emit paymentRequestsChanged();
}

int Wallet::findSecretFor(const Streaming::ConstBuffer &outputScript) const
{
    std::vector<std::vector<uint8_t> > vSolutions;
    Script::TxnOutType whichType;
    if (!Script::solver(outputScript, whichType, vSolutions))
        return -1;

    CKeyID keyID;
    switch (whichType)
    {
    case Script::TX_PUBKEY:
        keyID = CPubKey(vSolutions[0]).getKeyId();
        break;
    case Script::TX_PUBKEYHASH:
        keyID = CKeyID(uint160(vSolutions[0]));
        break;
    case Script::TX_SCRIPTHASH:
    case Script::TX_MULTISIG:
        // we don't store those in the wallet.
        return -1;
    case Script::TX_NONSTANDARD:
    case Script::TX_NULL_DATA:
        return -1;
    }

    for (auto i = m_walletSecrets.begin(); i != m_walletSecrets.end(); ++i) {
        if (i->second.address == keyID) {
            return i->first;
        }
    }
    return -1;
}

void Wallet::rebuildBloom()
{
    auto lock = m_segment->clearFilter();
    int unusedToInclude = 20;
    int hdUnusedToInclude = 10;
    int changeUnusedToInclude = 30;

    std::set<int> secretsUsed;
    for (const auto &i : m_walletTransactions) {
        const auto &wtx = i.second;
        for (const auto &output : wtx.outputs) {
            secretsUsed.insert(output.second.walletSecretId);
        }
    }

    std::set<int> secretsWithBalance;
    for (auto utxo : m_unspentOutputs) {
        auto i = m_walletTransactions.find(OutputRef(utxo.first).txIndex());
        Q_ASSERT(i != m_walletTransactions.end());
        const auto &wtx = i->second;
        if (i->second.minedBlockHeight > 1) {
            for (const auto &output : wtx.outputs) {
                secretsWithBalance.insert(output.second.walletSecretId);
            }
        }
    }
    logDebug() << "Rebuilding bloom filter. UTXO-size:" << secretsWithBalance.size();
    for (auto &i : m_walletSecrets) {
        const auto &secret = i.second;
        if (secret.initialHeight >= 10000000) {
            // is a timestamp, which means that we are waiting for the
            // headerSyncComplete() to be called and this key is fresh
            // and so searching in the history is useless.
            continue;
        }
        if (secret.reserved) {
            // whitelisted this one
        }
        else if (secret.fromHdWallet) {
            if (secret.fromChangeChain) {
                if (secretsUsed.find(i.first) == secretsUsed.end())  {
                    // never used before
                    if (--changeUnusedToInclude < 0)
                        continue;
                }
                /*
                 * The bloom filter gives us new transactions in future blocks.
                 * The change-chain is specifically designed to have one-time-use addresses.
                 * That means that after they become empty, we can stop looking for matches
                 * which avoids leaking privacy info.
                 *
                 * We skip all secrets that have been deposited-in at least once (used)
                 * and have no current balance on them (utxo does not refer to them)
                 */
                else if (secretsWithBalance.find(i.first) == secretsWithBalance.end()) {
                    continue;
                }
                m_hdData->lastIncludedChangeKey = std::max(m_hdData->lastIncludedChangeKey, secret.hdDerivationIndex);
            }
            // HD from main chain. If we never saw this, include a limited set.
            else {
                if (secretsUsed.find(i.first) == secretsUsed.end() && --hdUnusedToInclude < 0)
                    continue;
                m_hdData->lastIncludedMainKey = std::max(m_hdData->lastIncludedMainKey, secret.hdDerivationIndex);
            }
        }
        else if (--unusedToInclude < 0) {
            continue;
        }
        m_segment->addKeyToFilter(secret.address, secret.initialHeight);
    }
    for (auto utxo : m_unspentOutputs) {
        OutputRef ref(utxo.first);
        assert(m_walletTransactions.find(ref.txIndex()) != m_walletTransactions.end());
        auto tx = m_walletTransactions.at(ref.txIndex());
        m_segment->addToFilter(tx.txid, ref.outputIndex());
    }
    m_bloomScore = 0;
}

bool Wallet::isSingleAddressWallet() const
{
    return m_singleAddressWallet;
}

void Wallet::setSingleAddressWallet(bool singleAddressWallet)
{
    if (m_singleAddressWallet == singleAddressWallet)
        return;
    m_singleAddressWallet = singleAddressWallet;
    m_secretsChanged = true;
}

void Wallet::broadcastTxFinished(int txIndex, bool success)
{
    QMutexLocker locker(&m_lock);
    for (int i = 0; i < m_broadcastingTransactions.size(); ++i) {
        if (m_broadcastingTransactions.at(i)->txIndex() == txIndex) {
            m_broadcastingTransactions.removeAt(i);

            if (!success) {
                auto wtx = m_walletTransactions.find(txIndex);
                if (wtx != m_walletTransactions.end()) {
                    logCritical() << "Marking transaction invalid";
                    auto &tx = wtx->second;
                    if (tx.minedBlockHeight == WalletPriv::Unconfirmed) {
                        // a transaction that has been added before, but now marked
                        // rejected means we should revert some stuff that newTransaction() did.
                        // - locked output
                        // - utxo
                        // - balance

                        auto i = m_lockedOutputs.begin();
                        while (i != m_lockedOutputs.end()) {
                            if (i->second == txIndex)
                                i = m_lockedOutputs.erase(i);
                            else
                                ++i;
                        }
                        for (auto out = tx.outputs.begin(); out != tx.outputs.end(); ++out) {
                            auto utxo = m_unspentOutputs.find(OutputRef(txIndex, out->first).encoded());
                            assert(utxo != m_unspentOutputs.end());
                            m_unspentOutputs.erase(utxo);
                        }

                        recalculateBalance();
                    }
                    else {
                        assert(false); // Can't imagine the usecase, so if this hits in a debug build lets fail-fast
                        logWarning() << "Transaction marked rejected that had blockHeight:" << tx.minedBlockHeight;
                    }
                    tx.minedBlockHeight = WalletPriv::Rejected;
                }
            }
            return;
        }
    }
}

QString Wallet::name() const
{
    return m_name;
}

void Wallet::setName(const QString &name)
{
    QMutexLocker locker(&m_lock);
    m_name = name;
    m_walletChanged = true;
}

const uint256 &Wallet::txid(int txIndex) const
{
    QMutexLocker locker(&m_lock);
    auto iter = m_walletTransactions.find(txIndex);
    if (m_walletTransactions.end() == iter)
        throw std::runtime_error("Invalid tx-index");
    return iter->second.txid;
}

Tx::Output Wallet::txOutput(Wallet::OutputRef ref) const
{
    uint256 txid;
    {
        QMutexLocker locker(&m_lock);
        auto iter = m_walletTransactions.find(ref.txIndex());
        if (m_walletTransactions.end() == iter)
            throw std::runtime_error("Invalid ref");
        txid = iter->second.txid;
    }

    Tx tx = loadTransaction(txid, Streaming::pool(0));
    if (tx.size() == 0)
        throw std::runtime_error("missing TX data");
    return tx.output(ref.outputIndex());
}

qint64 Wallet::utxoOutputValue(OutputRef ref) const
{
    QMutexLocker locker(&m_lock);
    auto iter = m_unspentOutputs.find(ref.encoded());
    if (m_unspentOutputs.end() == iter)
        throw std::runtime_error("Invalid ref");
    return iter->second;
}

Wallet::PrivKeyData Wallet::unlockKey(Wallet::OutputRef ref) const
{
    QMutexLocker locker(&m_lock);
    auto iter = m_walletTransactions.find(ref.txIndex());
    if (m_walletTransactions.end() == iter)
        throw std::runtime_error("Invalid ref");
    auto iter2 = iter->second.outputs.find(ref.outputIndex());
    if (iter2 == iter->second.outputs.end())
        throw std::runtime_error("Invalid ref(2)");
    auto iter3 = m_walletSecrets.find(iter2->second.walletSecretId);
    assert(iter3 != m_walletSecrets.end());
    PrivKeyData rc;
    rc.sigType = iter3->second.signatureType;
    rc.privKeyId = iter3->first;
    rc.key = iter3->second.privKey;
    return rc;
}

void Wallet::updateSignatureType(const PrivKeyData &data)
{
    QMutexLocker locker(&m_lock);
    auto iter = m_walletSecrets.find(data.privKeyId);
    if (iter == m_walletSecrets.end())
        throw std::runtime_error("Invalid key-id");

    iter->second.signatureType = data.sigType;
}

CKeyID Wallet::nextUnusedAddress()
{
    CKeyID answer;
    reserveUnusedAddress(answer);
    return answer;
}

CKeyID Wallet::nextUnusedChangeAddress()
{
    CKeyID answer;
    reserveUnusedAddress(answer, ChangePath);
    return answer;
}

int Wallet::reserveUnusedAddress(CKeyID &keyId, PrivKeyType pkt)
{
    QMutexLocker locker(&m_lock);
    for (auto i = m_walletSecrets.begin(); i != m_walletSecrets.end(); ++i) {
        auto &secret = i->second;
        if (m_singleAddressWallet) {
            keyId = secret.address;
            return i->first; // just return the first then.
        }
        if (secret.reserved)
            continue;
        if (secret.signatureType != NotUsedYet) // used to sign, so obviously not unused.
            continue;
        if (secret.fromHdWallet) {
            // we skip the 'change' chain unless the app asked for it.
            if ((pkt == ChangePath) != secret.fromChangeChain)
                continue;
        }
        // The only place we remember if we received something for this private key is our tx list
        bool used = false;
        for (auto w = m_walletTransactions.cbegin(); !used && w != m_walletTransactions.cend(); ++w) {
            const auto &wtx = w->second;
            for (auto o = wtx.outputs.cbegin(); o != wtx.outputs.cend(); ++o) {
                if(o->second.walletSecretId == i->first) {
                    used = true;
                    break;
                }
            }
        }
        if (used)
            continue;
        secret.reserved = true;
        keyId = secret.address;
        rebuildBloom(); // make sure that we actually observe changes on this address
        return i->first;
    }

    // no unused addresses, lets make some.
    if (m_hdData.get()) {
        deriveHDKeys(50, 0);
        return reserveUnusedAddress(keyId);
    }

    int answer;
    for (int i = 0; i < 50; ++i) {
        WalletSecret secret;
        secret.privKey.makeNewKey();
        const CPubKey pubkey = secret.privKey.getPubKey();
        secret.address = pubkey.getKeyId();
        if (i == 0) {
            answer = m_nextWalletSecretId;
            keyId = secret.address;
        }
        m_walletSecrets.insert(std::make_pair(m_nextWalletSecretId++, secret));
    }
    m_secretsChanged = true;
    saveSecrets();

    return answer;
}

void Wallet::unreserveAddress(int index)
{
    QMutexLocker locker(&m_lock);
    auto f = m_walletSecrets.find(index);
    if (f != m_walletSecrets.end())
        f->second.reserved = false;
}

void Wallet::setLastSynchedBlockHeight(int height)
{
    if (m_lastBlockHeightSeen == height)
        return;
    m_walletChanged = true;
    m_lastBlockHeightSeen = height;
    emit lastBlockSynchedChanged();

    recalculateBalance();
    emit utxosChanged(); // in case there was an immature coinbase, this updates the balance

    if (height == FloweePay::instance()->headerChainHeight()) {
        // start this in my own thread and free of mutex-locks
        QTimer::singleShot(0, this, SLOT(broadcastUnconfirmed()));
    }
}

void Wallet::headerSyncComplete()
{
    /*
     * Private keys could have been added that do not have a start-blockheight
     * because the chain was not yet synched.
     * They will have a 'blockheight' that is in reality a timestamp. Trivial to
     * differentiate since that is above 1 trillion, whereas blocks will be around a million.
     *
     * If we have any such private keys, we can fix this.
     *   Find the matching block and update the bloom filter.
     */

    for (auto iter = m_walletSecrets.begin(); iter != m_walletSecrets.end(); ++iter) {
        if (iter->second.initialHeight > 10000000) {
            // this is a time based height, lets resolve it to a real height.
            const Blockchain &blockchain = FloweePay::instance()->p2pNet()->blockchain();
            iter->second.initialHeight = blockchain.blockHeightAtTime(iter->second.initialHeight);
            m_secretsChanged = true;
            rebuildBloom();
        }
    }
}

void Wallet::broadcastUnconfirmed()
{
    Q_ASSERT(thread() == QThread::currentThread());

    // we are (again) up-to-date.
    // Lets broadcast any transactions that have not yet been confirmed.
    QMutexLocker locker(&m_lock);
    m_broadcastingTransactions.clear();

    for (auto iter = m_walletTransactions.begin();
         iter != m_walletTransactions.end(); ++iter) {

        if (iter->second.minedBlockHeight == WalletPriv::Unconfirmed) {
            auto tx = loadTransaction(iter->second.txid, Streaming::pool(0));
            if (tx.data().size() > 64) {
                auto bc = std::make_shared<WalletInfoObject>(this, iter->first, tx);
                bc->moveToThread(thread());
                logDebug() << "  broadcasting transaction" << tx.createHash() << tx.size();
                m_broadcastingTransactions.append(bc);
#ifndef IN_TESTS // don't call singleton in unit tests
                FloweePay::instance()->p2pNet()->connectionManager().broadcastTransaction(bc);
#endif
            }
            else {
                logCritical() << "Unconfirmed transaction could not be found on disk!";
            }
        }
    }
}

PrivacySegment * Wallet::segment() const
{
    return m_segment.get();
}

void Wallet::createNewPrivateKey(uint32_t currentBlockheight)
{
    QMutexLocker locker(&m_lock);
    WalletSecret secret;
    if (m_hdData.get()) {
        secret.fromHdWallet = true;
        secret.hdDerivationIndex = ++m_hdData->lastMainKey;

        const auto count = m_hdData->derivationPath.size();
        assert(count >= 2);
        m_hdData->derivationPath[count - 2] = 0; // Flowee Pay doesn't really care about the 'change' chain.
        m_hdData->derivationPath[count - 1] = secret.hdDerivationIndex;
        secret.privKey = m_hdData->masterKey.derive(m_hdData->derivationPath);
    }
    else {
        secret.privKey.makeNewKey();
    }
    const CPubKey pubkey = secret.privKey.getPubKey();
    secret.address = pubkey.getKeyId();
    secret.initialHeight = currentBlockheight;
    m_walletSecrets.insert(std::make_pair(m_nextWalletSecretId++, secret));
    m_secretsChanged = true;
    saveSecrets();

    if (currentBlockheight < 10000000)
        // if its out of this range, likely its a timestamp (headers are not yet synched)
        rebuildBloom();
}

bool Wallet::addPrivateKey(const QString &privKey, uint32_t startBlockHeight)
{
    QMutexLocker locker(&m_lock);
    CBase58Data encodedData;
    auto bytes = privKey.toLatin1();
    encodedData.SetString(bytes.constData());
    if (encodedData.isMainnetPrivKey() || encodedData.isTestnetPrivKey()) {
        WalletSecret secret;
        secret.privKey.set(encodedData.data().begin(), encodedData.data().begin() + 32,
                encodedData.data().size() > 32 && encodedData.data()[32] == 1);

        // TODO loop over secrets and avoid adding one privkey twice.

        const CPubKey pubkey = secret.privKey.getPubKey();
        secret.address = pubkey.getKeyId();
        secret.initialHeight = startBlockHeight;
        m_walletSecrets.insert(std::make_pair(m_nextWalletSecretId++, secret));
        m_secretsChanged = true;
        saveSecrets();

        if (startBlockHeight < 10000000)
           rebuildBloom();
        return true;
    }
    logFatal() << "ERROR. Wallet: added string is not a private key";
    return false;
}

Streaming::ConstBuffer Wallet::readSecrets() const
{
    std::ifstream in((m_basedir / "secrets.dat").string());
    if (!in.is_open())
        throw std::runtime_error("Missing secrets.dat");
    const auto dataSize = boost::filesystem::file_size(m_basedir / "secrets.dat");
    auto &pool = Streaming::pool(dataSize);
    in.read(pool.begin(), dataSize);
    return pool.commit(dataSize);
}

void Wallet::loadSecrets()
{
    QMutexLocker locker(&m_lock);
    auto fileData = readSecrets();
    if (m_encryptionSeed) {
        // that gives a good chance that we are dealing with an encrypted
        // wallet. Now the question is, do we have a fully encrypted wallet or a partial one.
        // On top of that we need to figure out if we have a key to decrypt it, or if we
        // simply return without loading.
        //    Also see decrypt()

        Streaming::MessageParser checker(fileData);
        while (m_encryptionLevel == NotEncrypted) {
            auto rc = checker.next();
            if (rc == Streaming::Error || checker.tag() >= WalletPriv::END_OF_PRIV_SAVE_TAGS)
                m_encryptionLevel = FullyEncrypted;
            else if (rc == Streaming::EndOfDocument)
                break;
        }
        if (m_encryptionLevel == FullyEncrypted) {
            if (!m_haveEncryptionKey)
                return;
            // we decrypt it.
            AES256CBCDecrypt crypto(&m_encryptionKey[0], &m_encryptionIR[0], true);
            Streaming::BufferPool pool(fileData.size());
            int newSize = crypto.decrypt(fileData.begin(), fileData.size(), pool.data());
            if (newSize == 0) {
                logCritical() << "Reading (encrypted) secrets file failed";
                return;
            }
            fileData = pool.commit(newSize);
        }
    }
    Streaming::MessageParser parser(fileData);
    WalletSecret secret;
    std::string mnemonic, mnemonicpwd;
    std::vector<uint32_t> derivationPath;
    int derivationPathChangeIndex = -1;
    int derivationPathMainIndex = -1;
    int index = 0;
    while (parser.next() == Streaming::FoundTag) {
        if (parser.tag() == WalletPriv::Separator) {
            if (index > 0 && secret.address.size() > 0) {
                m_walletSecrets.insert(std::make_pair(index, secret));
                m_nextWalletSecretId = std::max(m_nextWalletSecretId, index);
#if 0
                CashAddress::Content c;
                c.hash.resize(20);
                memcpy(c.hash.data(), secret.address.begin(), 20);
                c.type = CashAddress::PUBKEY_TYPE;
                auto ad = CashAddress::encodeCashAddr("bitcoincash", c);
                logCritical() << "Loaded" << index << ad;
#endif
            }
            secret = WalletSecret();
        }
        else if (parser.tag() == WalletPriv::Index) {
            index = parser.intData();
        }
        else if (parser.tag() == WalletPriv::PrivKey) {
            auto d = parser.unsignedBytesData();
            secret.privKey.set(d.begin(), d.end(), true);
        }
        else if (parser.tag() == WalletPriv::PrivKeyEncrypted) {
            m_encryptionLevel = SecretsEncrypted;
        }
        else if (parser.tag() == WalletPriv::PubKeyHash) {
            auto d = parser.bytesDataBuffer();
            secret.address = CKeyID(d.begin());
        }
        else if (parser.tag() == WalletPriv::HeightCreated) {
            if (parser.intData() == -1) // legacy indicator of 'unused'. (changed in 2021.05)
                secret.initialHeight = 0;
            else
                secret.initialHeight = parser.longData();
        }
        else if (parser.tag() == WalletPriv::HDKeyFromChangeChain) {
            secret.fromHdWallet = true;
            secret.fromChangeChain = parser.boolData();
        }
        else if (parser.tag() == WalletPriv::HDKeyDerivationIndex) {
            secret.fromHdWallet = true;
            secret.hdDerivationIndex = parser.intData();
        }

        else if (parser.tag() == WalletPriv::IsSingleAddressWallet) {
            m_singleAddressWallet = parser.boolData();
        }
        else if (parser.tag() == WalletPriv::UserOwnedWallet) {
            m_userOwnedWallet = parser.boolData();
        }
        else if (parser.tag() == WalletPriv::SignatureType) {
            secret.signatureType = static_cast<SignatureType>(parser.intData());
        }
        else if (parser.tag() == WalletPriv::HDWalletMnemonic) {
            mnemonic = parser.stringData();
        }
        else if (parser.tag() == WalletPriv::HDWalletMnemonicPassword) {
            mnemonicpwd = parser.stringData();
        }
        else if (parser.tag() == WalletPriv::HDWalletPathItem) {
            derivationPath.push_back(static_cast<uint32_t>(parser.longData()));
        }
        else if (parser.tag() == WalletPriv::HDWalletLastChangeIndex) {
            derivationPathChangeIndex = parser.intData();
        }
        else if (parser.tag() == WalletPriv::HDWalletLastReceiveIndex) {
            derivationPathMainIndex = parser.intData();
        }
        else if (parser.tag() == WalletPriv::WalletVersion) {
            m_walletVersion = parser.intData();
        }
    }

    if (mnemonic.empty() != derivationPath.empty()) {
        logFatal() << "Found incomplete data for HD wallet";
    } else if (!mnemonic.empty()) {
        assert(m_hdData.get() == nullptr);
        try {
            m_hdData.reset(new HierarchicallyDeterministicWalletData(mnemonic, mnemonicpwd));
            m_hdData->derivationPath = derivationPath;
            m_hdData->lastChangeKey = derivationPathChangeIndex;
            m_hdData->lastMainKey = derivationPathMainIndex;
        } catch (const std::exception &e) {
            logFatal() << "Failed loading the HD wallet due to:" << e;
        }
    }

    m_secretsChanged = false;
    ++m_nextWalletSecretId;
}

void Wallet::saveSecrets()
{
    // mutex already locked
    if (!m_secretsChanged)
        return;
    int hdDataSize = 0;
    if (m_hdData.get()) {
        assert (!m_hdData->walletMnemonic.isEmpty());
        assert (m_hdData->masterKey.isValid());
        // Reserve enough space. Notice that string-length and encoded length may be different.
        hdDataSize = m_hdData->walletMnemonic.length() * 5 + m_hdData->walletMnemonicPwd.length() * 5;
        hdDataSize += m_hdData->walletMnemonicPwd.length() * 5;
        hdDataSize += m_hdData->derivationPath.size() * 6;
    }

    Streaming::BufferPool pool(m_walletSecrets.size() * 70 + hdDataSize);
    Streaming::MessageBuilder builder(pool);

    std::unique_ptr<AES256CBCEncrypt> crypto;
    if (m_encryptionLevel >= SecretsEncrypted) {
        if (!m_haveEncryptionKey)
            throw std::runtime_error("Can not save wallet encrypted, no password set");
        crypto.reset(new AES256CBCEncrypt(&m_encryptionKey[0], &m_encryptionIR[0],
                m_encryptionLevel == FullyEncrypted));
    }
    builder.add(WalletPriv::WalletVersion, m_walletVersion);
    if (m_hdData.get()) {
        builder.add(WalletPriv::HDWalletMnemonic, m_hdData->walletMnemonic.toUtf8().constData());
        if (!m_hdData->walletMnemonicPwd.isEmpty())
            builder.add(WalletPriv::HDWalletMnemonicPassword, m_hdData->walletMnemonicPwd.toUtf8().constData());
        for (const uint32_t item : m_hdData->derivationPath) {
            builder.add(WalletPriv::HDWalletPathItem,  static_cast<uint64_t>(item));
        }
        if (m_hdData->lastChangeKey >= 0)
            builder.add(WalletPriv::HDWalletLastChangeIndex,  m_hdData->lastChangeKey);
        if (m_hdData->lastMainKey >= 0)
            builder.add(WalletPriv::HDWalletLastReceiveIndex,  m_hdData->lastMainKey);
    }
    for (const auto &item : m_walletSecrets) {
        const auto &secret = item.second;
        builder.add(WalletPriv::Index, item.first);
        assert(secret.privKey.isValid());
        if (m_encryptionLevel == SecretsEncrypted) {
            char buf[32]; // private keys are always 32 bytes
            crypto->encrypt(reinterpret_cast<const char*>(secret.privKey.begin()), sizeof(buf), buf);
            builder.addByteArray(WalletPriv::PrivKeyEncrypted, buf, sizeof(buf));
        }
        else {
            builder.addByteArray(WalletPriv::PrivKey, secret.privKey.begin(), secret.privKey.size());
        }
        builder.addByteArray(WalletPriv::PubKeyHash, secret.address.begin(), secret.address.size());
        if (secret.initialHeight > 0)
            builder.add(WalletPriv::HeightCreated, (uint64_t) secret.initialHeight);
        if (item.second.signatureType != NotUsedYet)
            builder.add(WalletPriv::SignatureType, secret.signatureType);
        if (secret.fromHdWallet) {
            if (secret.fromChangeChain)
                builder.add(WalletPriv::HDKeyFromChangeChain, true);
            builder.add(WalletPriv::HDKeyDerivationIndex, secret.hdDerivationIndex);
        }
        builder.add(WalletPriv::Separator, true);
    }
    if (m_singleAddressWallet)
        builder.add(WalletPriv::IsSingleAddressWallet, true);
    if (!m_userOwnedWallet)
        builder.add(WalletPriv::UserOwnedWallet, false);
    auto data = builder.buffer();

    try {
        boost::filesystem::create_directories(m_basedir);
        boost::filesystem::remove(m_basedir / "secrets.dat~");

        if (m_encryptionLevel == FullyEncrypted) {
            pool.reserve(data.size() + AES_BLOCKSIZE);
            int size = crypto->encrypt(data.begin(), data.size(), pool.data());
            data = pool.commit(size);
        }
        std::ofstream outFile((m_basedir / "secrets.dat~").string());
        outFile.write(data.begin(), data.size());
        outFile.flush();
        outFile.close();
        boost::filesystem::rename(m_basedir / "secrets.dat~", m_basedir / "secrets.dat");
    } catch (const std::exception &e) {
        logFatal() << "Failed to save the database. Reason:" << e.what();
    }
    m_secretsChanged = false;
}

void Wallet::delayedSave()
{
    assert(thread() == QThread::currentThread());
    if (m_saveStarted)
        return;
    m_saveStarted = true;

    QTimer::singleShot(1200, this, SLOT(delayedSaveTimeout()));
}

void Wallet::loadWallet()
{
    std::ifstream in((m_basedir / "wallet.dat").string());
    if (!in.is_open())
        return;
    QMutexLocker locker(&m_lock);
    auto dataSize = boost::filesystem::file_size(m_basedir / "wallet.dat");
    Streaming::BufferPool pool(dataSize);
    in.read(pool.begin(), dataSize);
    if (m_encryptionLevel == FullyEncrypted) {
        if (!m_haveEncryptionKey)
            return;

        // we decrypt it.
        auto data = pool.commit(dataSize);
        AES256CBCDecrypt crypto(&m_encryptionKey[0], &m_encryptionIR[0], true);
        pool.reserve(dataSize);
        dataSize = crypto.decrypt(data.begin(), data.size(), pool.data());
        if (dataSize == 0) {
            logCritical() << "Reading (encrypted) wallet.dat file failed";
            return;
        }
    }
    Streaming::MessageParser parser(pool.commit(dataSize));
    WalletTransaction wtx;
    int index = 0;
    int inputIndex = -1;
    int outputIndex = -1;
    int tmp = 0;
    Output output;
    QSet<int> newTx;
    int highestBlockHeight = 0;
    PaymentRequest *pr = nullptr;
    OutputRef paymentRequestRef;
    while (parser.next() == Streaming::FoundTag) {
        if (parser.tag() == WalletPriv::Separator) {
            assert(index > 0);
            assert(!wtx.inputToWTX.empty() || !wtx.outputs.empty());
            m_walletTransactions.insert(std::make_pair(index, wtx));
            m_txidCache.insert(std::make_pair(wtx.txid, index));
            m_nextWalletTransactionId = std::max(m_nextWalletTransactionId, index);
            // insert outputs of new tx.
            for (auto i = wtx.outputs.begin(); i != wtx.outputs.end(); ++i) {
                OutputRef ref(index, i->first);
                m_unspentOutputs.insert(std::make_pair(ref.encoded(), i->second.value));
            }
            newTx.insert(index);

#ifdef DEBUGUTXO
            logFatal() << "Wallet has tx: " << wtx.txid << "@" << wtx.minedBlockHeight;
            for (auto pair : wtx.outputs) {
                logFatal() << "  ++ " << pair.first << pair.second.value << "sat";
            }
            for (auto pair : wtx.inputToWTX) {
                OutputRef ref(pair.second);
                logFatal() << "  -- " << pair.first << ref.txIndex() << ref.outputIndex();
                auto w = m_walletTransactions.find(ref.txIndex());
                if (w != m_walletTransactions.end())
                    logFatal() << "     " << w->second.txid;
            }
#endif

            wtx = WalletTransaction();
            inputIndex = -1;
            outputIndex = -1;
            output = Output();
            index = -1;
        }
        else if (parser.tag() == WalletPriv::Index) {
            index = parser.intData();
        }
        else if (parser.tag() == WalletPriv::TxId) {
            wtx.txid = parser.uint256Data();
        }
        else if (parser.tag() == WalletPriv::BlockHash) {
            wtx.minedBlock = parser.uint256Data();
        }
        else if (parser.tag() == WalletPriv::BlockHeight) {
            wtx.minedBlockHeight = parser.intData();
            highestBlockHeight = std::max(parser.intData(), highestBlockHeight);
        }
        else if (parser.tag() == WalletPriv::OutputFromCoinbase) {
            wtx.isCoinbase = parser.boolData();
        }
        else if (parser.tag() == WalletPriv::TxIsCashFusion) {
            wtx.isCashFusionTx = parser.boolData();
        }
        else if (parser.tag() == WalletPriv::InputIndex) {
            inputIndex = parser.intData();
        }

        // chaining of transactions, we always expect a pair of InputSpentsTx and InputSpendsOutput
        else if (parser.tag() == WalletPriv::InputSpendsTx) {
            tmp = parser.intData();
        }
        else if (parser.tag() == WalletPriv::InputSpendsOutput) {
            assert(inputIndex >= 0);
            assert(tmp != 0);
            assert(parser.longData() < 0xFFFF);
            OutputRef ref(tmp, parser.intData());
            wtx.inputToWTX.insert(std::make_pair(inputIndex, ref.encoded()));
            tmp = 0;
        }
        else if (parser.tag() == WalletPriv::OutputIndex) {
            outputIndex = parser.intData();
        }
        else if (parser.tag() == WalletPriv::OutputValue) {
            output.value = parser.longData();
        }

        // an ouput can get locked, stopping the user from spending it.
        else if (parser.tag() == WalletPriv::OutputLockState) {
            WalletPriv::OutputLockStateEnum inputLock = static_cast<WalletPriv::OutputLockStateEnum>(parser.intData());
            if (inputLock == WalletPriv::UserLocked) { // we handle the 'auto-locked' case in the OutputLockAutoSpender
                // ref is made up of WalletPriv::Index and OutputIndex
                OutputRef ref(index, outputIndex);
                m_lockedOutputs.insert(std::make_pair(ref.encoded(), 0));
            }
        }
        else if (parser.tag() == WalletPriv::OutputLockAutoSpender) {
            // ref is made up of WalletPriv::Index and OutputIndex
            OutputRef ref(index, outputIndex);
            m_lockedOutputs.insert(std::make_pair(ref.encoded(), parser.intData()));
        }

        else if (parser.tag() == WalletPriv::KeyStoreIndex) {
            assert(outputIndex >= 0);
            assert(output.value > 0);
            output.walletSecretId = parser.intData();
            wtx.outputs.insert(std::make_pair(outputIndex, output));

            if (wtx.minedBlockHeight == -1) {
                /*
                 * Unconfirmed transactions may be received ones for an address we shared in a previous
                 * session. In that case we want to mark the address as 'reserved' in order to avoid
                 * a new paymentRequest using the same address.
                 */
                auto i = m_walletSecrets.find(output.walletSecretId);
                assert(i != m_walletSecrets.end());
                if (i != m_walletSecrets.end()) // sanity check, don't completely trust loading.
                    i->second.reserved = true;
            }
        }
        else if (parser.tag() == WalletPriv::WalletName) {
            assert(parser.isString());
            m_name = QString::fromUtf8(parser.stringData().c_str(), parser.dataLength());
        }
        else if (parser.tag() == WalletPriv::UserComment) {
            assert(parser.isString());
            wtx.userComment = QString::fromUtf8(parser.stringData().c_str(), parser.dataLength());
        }
        else if (parser.tag() == WalletPriv::LastSynchedBlock) {
            highestBlockHeight = std::max(parser.intData(), highestBlockHeight);
        }
        else if (parser.tag() == WalletPriv::PaymentRequestType) {
            pr = new PaymentRequest(this, parser.intData());
            m_paymentRequests.append(pr);
            paymentRequestRef = OutputRef();
        }
        else if (parser.tag() == WalletPriv::PaymentRequestAddress) {
            // we assert on pr being null here and below based on the idea that the loaded file
            // is private and trusted. The asserts are here to make sure that the saving code
            // matches the loading, in production there is then no need to doubt the correctness
            // of the loaded data.
            assert(pr);
            pr->m_privKeyId = parser.intData();
            auto i = m_walletSecrets.find(pr->m_privKeyId);
            if (i != m_walletSecrets.end()) {
                i->second.reserved = true;
            } else {
                logFatal() << "PaymentRequest refers to non-existing wallet-secret!";
            }
        }
        else if (parser.tag() == WalletPriv::PaymentRequestMessage) {
            assert(pr);
            auto data = parser.bytesDataBuffer();
            pr->m_message = QString::fromUtf8(data.begin(), data.size());
        }
        else if (parser.tag() == WalletPriv::PaymentRequestAmount) {
            assert(pr);
            pr->m_amountRequested = parser.longData();
        }
        else if (parser.tag() == WalletPriv::PaymentRequestOldAddress) {
            assert(pr);
            pr->m_useLegacyAddressFormat = parser.boolData();
        }
        else if (parser.tag() == WalletPriv::PaymentRequestTxIndex) {
            paymentRequestRef.setTxIndex(parser.intData());
        }
        else if (parser.tag() == WalletPriv::PaymentRequestOutputIndex) {
            paymentRequestRef.setOutputIndex(parser.intData());
            assert(pr);
            pr->m_incomingOutputRefs.append(paymentRequestRef.encoded());
        }
        else if (parser.tag() == WalletPriv::PaymentRequestPaid) {
            assert(pr);
            pr->m_amountSeen = parser.longData();
            if (pr->m_amountSeen >= pr->m_amountRequested)
                pr->m_paymentState = PaymentRequest::PaymentSeenOk;
        }
    }

    // after inserting all outputs during load, now remove all inputs these tx's spent.
    for (auto index : newTx) {
        auto iter = m_walletTransactions.find(index);
        assert(iter != m_walletTransactions.end());

        if (iter->second.minedBlockHeight == WalletPriv::Rejected)
            continue;
        if (iter->second.minedBlockHeight != WalletPriv::Unconfirmed) {
            assert(iter->second.minedBlockHeight > 0);
            // remove UTXOs this Tx spent
            for (auto i = iter->second.inputToWTX.begin(); i != iter->second.inputToWTX.end(); ++i) {
                auto utxo = m_unspentOutputs.find(i->second);
                assert(utxo != m_unspentOutputs.end()); // Loading should be done in-order to avoid this.
                if (utxo != m_unspentOutputs.end())
                    m_unspentOutputs.erase(utxo);
            }
        }
    }
    if (highestBlockHeight > 0) {
        m_segment->blockSynched(highestBlockHeight);
        m_segment->blockSynched(highestBlockHeight); // yes, twice.
    } else {
        // otherwise the blockSynced() implicitly calls this.
        recalculateBalance();
    }

#ifdef DEBUGUTXO
    for (auto output : m_unspentOutputs) {
        OutputRef ref(output.first);
        auto utxo = m_walletTransactions.find(ref.txIndex());
        assert(utxo != m_walletTransactions.end());
        auto out = utxo->second.outputs.find(ref.outputIndex());
        assert(out != utxo->second.outputs.end());
        assert(out->second.value == output.second);
        logFatal() << "Unspent: " << utxo->second.txid << ref.outputIndex() << "\t->" << out->second.value << "sats";

        auto locked = m_lockedOutputs.find(ref.encoded());
        if (locked != m_lockedOutputs.end()) {
            logFatal() << "      \\=  Locked UTXO" << locked->second;
        }
    }
#endif

#ifndef NDEBUG
    // sanity check: the inputs should resolve to transactions in our list.
    for (auto &tx : m_walletTransactions) {
        for (auto in : tx.second.inputToWTX) {
            auto key = in.second;
            int outputIndex = key & 0xFFFF;
            key >>= 16;
            assert(m_walletTransactions.find(key) != m_walletTransactions.end());
            auto spendingTx = m_walletTransactions.at(key);
            assert(spendingTx.outputs.find(outputIndex) != spendingTx.outputs.end());
        }
    }

    // check sanity of autoLockedOutputs struct
    for (auto &pair : m_lockedOutputs) {
        auto utxoLink = m_unspentOutputs.find(pair.first);
        assert(utxoLink != m_unspentOutputs.end());
        assert(pair.second >= -1);
        if (pair.second > 0) { // zero means its user-locked, -1 is spam-locked
            auto w = m_walletTransactions.find(pair.second);
            assert (w != m_walletTransactions.end());
        }
    }
#endif
    m_walletChanged = false;
    // correct for bad input in wallet file
    for (auto i = m_lockedOutputs.begin(); i != m_lockedOutputs.end();) {
        auto utxoIter = m_unspentOutputs.find(i->first);
        if (utxoIter == m_unspentOutputs.end()) {
            logCritical() << "Found faulty 'locked' output-ref, dropping";
            i = m_lockedOutputs.erase(i); // this should never happen, cleanup
            m_walletChanged = true;
        } else {
            ++i;
        }
    }

    if (!m_walletTransactions.empty())
        ++m_nextWalletTransactionId;
    emit utxosChanged();
}

void Wallet::saveWallet()
{
    QMutexLocker locker(&m_lock);
    if (!m_walletChanged) {
        bool changed = false;
        for (auto i = m_paymentRequests.begin(); !changed && i != m_paymentRequests.end(); ++i) {
            changed = (*i)->m_dirty;
        }
        if (!changed)
            return;
    }

    if (m_encryptionLevel == FullyEncrypted && !m_haveEncryptionKey)
        throw std::runtime_error("Can not save without passphrase set");

    int saveFileSize = m_name.size() * 3 + 100;
    for (const auto &i : m_walletTransactions) {
        const auto &wtx = i.second;
        saveFileSize += 110 + wtx.inputToWTX.size() * 22 + wtx.outputs.size() * 30;
    }
    for (const auto &pr : qAsConst(m_paymentRequests)) {
        saveFileSize += 100 + pr->m_message.size() * 6 + pr->m_incomingOutputRefs.size() * 12;
    }

    Streaming::BufferPool pool(saveFileSize);
    Streaming::MessageBuilder builder(pool);
    for (const auto &item : m_walletTransactions) {
        builder.add(WalletPriv::Index, item.first);
        builder.add(WalletPriv::TxId, item.second.txid);
        builder.add(WalletPriv::BlockHash, item.second.minedBlock);
        builder.add(WalletPriv::BlockHeight, item.second.minedBlockHeight);
        if (item.second.isCoinbase)
            builder.add(WalletPriv::OutputFromCoinbase, true);
        if (item.second.isCashFusionTx)
            builder.add(WalletPriv::TxIsCashFusion, true);
        // Save all links we established when inputs spent outputs also in this wallet.
        for (auto i = item.second.inputToWTX.begin(); i != item.second.inputToWTX.end(); ++i) {
            builder.add(WalletPriv::InputIndex, i->first);
            const OutputRef ref(i->second);
            builder.add(WalletPriv::InputSpendsTx, ref.txIndex());
            builder.add(WalletPriv::InputSpendsOutput, ref.outputIndex()); // input that refers to the output in the WTX it spends
        }
        for (auto i = item.second.outputs.begin(); i != item.second.outputs.end(); ++i) {
            builder.add(WalletPriv::OutputIndex, i->first);
            builder.add(WalletPriv::OutputValue, i->second.value);
            builder.add(WalletPriv::KeyStoreIndex, i->second.walletSecretId);

            // outputs that have been locked for some reason.
            // One reason is we spent it already but that tx has not yet been confirmed. Otherwise it could be user-decided.
            auto lock = m_lockedOutputs.find(OutputRef(item.first, i->first).encoded());
            if (lock != m_lockedOutputs.end()) {
                builder.add(WalletPriv::OutputLockState, lock->second == 0 ? WalletPriv::UserLocked : WalletPriv::AutoLocked);
                if (lock->second != 0)
                    builder.add(WalletPriv::OutputLockAutoSpender, lock->second);
            }
        }
        if (!item.second.userComment.isEmpty())
            builder.add(WalletPriv::UserComment, item.second.userComment.toStdString());
        builder.add(WalletPriv::Separator, true);
    }
    builder.add(WalletPriv::LastSynchedBlock, m_segment->lastBlockSynched());
    builder.add(WalletPriv::WalletName, m_name.toUtf8().toStdString());

    for (auto pr : qAsConst(m_paymentRequests)) {
        builder.add(WalletPriv::PaymentRequestType, 0); // bip21 is the only one supported right now
        builder.add(WalletPriv::PaymentRequestAddress, pr->m_privKeyId);
        if (!pr->m_message.isEmpty())
            builder.add(WalletPriv::PaymentRequestMessage, pr->m_message.toUtf8().constData());
        assert(pr->m_amountRequested >= 0); // never negative
        if (pr->m_amountRequested > 0)
            builder.add(WalletPriv::PaymentRequestAmount, (uint64_t) pr->m_amountRequested);
        if (pr->m_useLegacyAddressFormat)
            builder.add(WalletPriv::PaymentRequestOldAddress, true);

        for (auto outRefNum : qAsConst(pr->m_incomingOutputRefs)) {
            OutputRef outRef(outRefNum);
            builder.add(WalletPriv::PaymentRequestTxIndex, outRef.txIndex());
            builder.add(WalletPriv::PaymentRequestOutputIndex, outRef.outputIndex());
        }
        if (pr->m_amountSeen > 0)
            builder.add(WalletPriv::PaymentRequestPaid, (uint64_t) pr->m_amountSeen);
    }

    auto data = builder.buffer();

    try {
        boost::filesystem::create_directories(m_basedir);
        boost::filesystem::remove(m_basedir / "wallet.dat~");

        if (m_encryptionLevel == FullyEncrypted) {
            AES256CBCEncrypt crypto(&m_encryptionKey[0], &m_encryptionIR[0], true);
            pool.reserve(data.size() + AES_BLOCKSIZE);
            int size = crypto.encrypt(data.begin(), data.size(), pool.data());
            data = pool.commit(size);
        }
        
        std::ofstream outFile((m_basedir / "wallet.dat~").string());
        outFile.write(data.begin(), data.size());
        outFile.flush();
        outFile.close();
        boost::filesystem::rename(m_basedir / "wallet.dat~", m_basedir / "wallet.dat");
    } catch (const std::exception &e) {
        logFatal() << "Failed to save the database. Reason:" << e.what();
    }
    m_walletChanged = false;
}

void Wallet::recalculateBalance()
{
    QMutexLocker locker(&m_lock);
    qint64 balanceConfirmed = 0;
    qint64 balanceImmature = 0;
    qint64 balanceUnconfirmed = 0;
    for (auto utxo : m_unspentOutputs) {
        auto wtx = m_walletTransactions.find(OutputRef(utxo.first).txIndex());
        Q_ASSERT(wtx != m_walletTransactions.end());
        const int h = wtx->second.minedBlockHeight;
        if (h == WalletPriv::Rejected)
            continue;
        else if (m_lockedOutputs.find(utxo.first) != m_lockedOutputs.end())
            continue;
        else if (h == WalletPriv::Unconfirmed)
            balanceUnconfirmed += utxo.second;
        else if (wtx->second.isCoinbase && h + MATURATION_AGE > m_lastBlockHeightSeen)
            balanceImmature += utxo.second;
        else
            balanceConfirmed += utxo.second;
    }
    if (m_balanceConfirmed == balanceConfirmed
            && m_balanceImmature == balanceImmature
            && m_balanceUnconfirmed == balanceUnconfirmed)
        return;
    m_balanceConfirmed = balanceConfirmed;
    m_balanceImmature = balanceImmature;
    m_balanceUnconfirmed = balanceUnconfirmed;
    emit balanceChanged();
}

void Wallet::delayedSaveTimeout()
{
    QMutexLocker locker(&m_lock);
    m_saveStarted = false;
    saveSecrets();
    saveWallet();
}

int Wallet::unspentOutputCount() const
{
    QMutexLocker locker(&m_lock);
    return m_unspentOutputs.size();
}

int Wallet::historicalOutputCount() const
{
    QMutexLocker locker(&m_lock);
    int count = 0;
    for (auto &wtx : m_walletTransactions) {
        count += wtx.second.outputs.size();
    }
    return count;
}
