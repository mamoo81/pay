/*
 * This file is part of the Flowee project
 * Copyright (C) 2020-2024 Tom Zander <tom@flowee.org>
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
#include "TxInfoObject.h"

#include <NotificationListener.h>
#include <primitives/script.h>
#include <streaming/BufferPool.h>
#include <streaming/BufferPools.h>
#include <streaming/MessageBuilder.h>
#include <streaming/MessageParser.h>
#include <base58.h>
#include <crypto/aes.h>

#include <QFile>
#include <QSet>
#include <QTimer>
#include <QThread>

#include <fstream>
#include <cassert>

// #define DEBUGUTXO

// static
Wallet *Wallet::createWallet(const boost::filesystem::path &basedir, uint16_t segmentId, const QString &name)
{
    Wallet *wallet = new Wallet();
    wallet->m_basedir = basedir / QString("wallet-%1/").arg(segmentId).toStdString();
    wallet->m_segment.reset(new PrivacySegment(segmentId, wallet));
    if (name.isEmpty())
        wallet->setName(QString("unnamed-%1").arg(segmentId));
    else
        wallet->setName(name);

    return wallet;
}

// the 'gap' used in bloom filters of net yet used addresses to send.
// this is a bit larger than others may use because we use a filter for a series
// of blocks that can hold a large number of our transactions.
constexpr int filter_unusedToInclude = 40;
/*
 * 'main' addresses are the ones the user explicitly requests and we give as a QR
 * in order to give to people. This has the effect that we can never stop asking
 * servers if any new deposits are made. People may use this address for yearly
 * contributions, for instance. So all main addresses always have to be included
 * in the bloom filter.
 *
 * This is additionally not a privacy matter as unused addresses are not possible
 * to get out of a bloom filter. Its a MATCHING-filter, afterall.
 * So our gap here is much much larger than for change addresses: safe and secure.
 */
constexpr int filter_hdUnusedToInclude = 200;
constexpr int filter_changeUnusedToInclude = 80;

Wallet::Wallet()
    : m_walletChanged(true),
    m_walletVersion(2)
{
    connect (this, SIGNAL(startDelayedSave()), this, SLOT(delayedSave()), Qt::QueuedConnection); // ensure right thread calls us.
}

Wallet::Wallet(const boost::filesystem::path &basedir, uint16_t segmentId, uint32_t encryptionSeed)
    : m_segment(new PrivacySegment(segmentId, this)),
    m_walletVersion(1), // after version 1 we saved the version in the secrets file.
    m_basedir(basedir / QString("wallet-%1/").arg(segmentId).toStdString()),
    m_encryptionSeed(encryptionSeed)
{
    // first try to load the wallet-name file.
    try {
        std::ifstream in((m_basedir / "name").string());
        if (in.is_open()) {
            char buf[100];
            in.read(buf, 100);
            m_name = QString::fromUtf8(buf, in.gcount());
            in.close();
        }
    } catch (const std::exception &e) {
        logInfo(LOG_WALLET) << "Failed to read a wallet-name file" << e;
    }

    loadSecrets();
    loadWallet();

    if (m_encryptionLevel == FullyEncrypted) {
        assert(!isDecrypted()); // because, how?
        m_segment->setEnabled(false);
    }
    rebuildBloom(ForceBuild);
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
    logDebug(LOG_WALLET) << "new tx." << wtx.txid;
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
                    logDebug(LOG_WALLET) << "  Input:" << inputIndex << "prevTx:" << prevTxhash
                               << Log::Hex << i->second << prevTx.encoded();
            }
        } else  if (iter.tag() == Tx::PrevTxIndex) {
            if (prevTx.isValid()) {
                assert(iter.longData() < 0xFFFF); // it would break our scheme
                prevTx.setOutputIndex(iter.intData());
                auto utxo = m_unspentOutputs.find(prevTx.encoded());
                if (utxo != m_unspentOutputs.end()) {
                    // input is spending one of our UTXOs
                    logDebug(LOG_WALLET) << "   -> spent UTXO";
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
            output.walletSecretId = findSecretFor(iter.byteData(), output.holdsCashToken);
            if (output.walletSecretId > 0) {
                logDebug(LOG_WALLET) << "   output"<< outputIndex << "pays to wallet id" << output.walletSecretId;
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
                    logDebug(LOG_WALLET) << "Transaction" << txid << "is a Cash_Fusion transaction.";
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
                .arg(FloweePay::amountToString(sats, FloweePay::BCH));
    }
    return wtx;
}

// check if we need to create more private keys based on if this transaction
// used private keys close to the index we have created and keep track off.
bool Wallet::updateHDSignatures(const Wallet::WalletTransaction &wtx, bool &updateBloom)
{
    // mutex locked by caller
    if (m_hdData.get() == nullptr)
        return false;

    static constexpr int ExtraAddresses = 100;
    int needChangeAddresses = 0;
    int needMainAddresses = 0;

    const int changeGap = filter_changeUnusedToInclude + (m_walletStoresCashFusions ? 50 : 0);

    for (auto i = wtx.outputs.begin(); i != wtx.outputs.end(); ++i) {
        auto privKey = m_walletSecrets.find(i->second.walletSecretId);
        assert(privKey != m_walletSecrets.end()); // invalid wtx
        const WalletSecret &ws = privKey->second;

        if (ws.fromHdWallet) {
            /*
             * We previously sent a list of addresses to the peers in the bloom filter,
             * we do this sequentually by HD-derivation index. We try to be conservative
             * in what we share, which means that as those addresses are used we may need
             * to send more addresses by rebuilding the bloom filter.
             *
             * This code looks at the 'gap', the amount of addresses between what is used
             * and what we previously send in our bloom filter. If the gap is getting too small,
             * we need to create a new filter in order to avoid missing transactions.
             */
            if (ws.fromChangeChain) {
                assert(m_hdData->lastChangeKey >= ws.hdDerivationIndex);
                if (m_hdData->lastChangeKey - ws.hdDerivationIndex < changeGap) {
                    needChangeAddresses = ExtraAddresses
                            + (m_walletStoresCashFusions ? 75 : 0);
                }
                // notice that here we trigger on there only being 30 addresses between what we used and
                // what was sent, while what we sent is (as set in filter_changeUnusedToInclude) much
                // heigher. This is intentional so we update the filter only when we used a larger list
                // of addresses instead of refreshing the filter after every single new key being used.
                updateBloom |= m_hdData->lastIncludedChangeKey - ws.hdDerivationIndex < 30; // the gap
            } else {
                assert(m_hdData->lastMainKey >= ws.hdDerivationIndex);
                updateBloom |= m_hdData->lastIncludedMainKey - ws.hdDerivationIndex < 50; // the gap
                // additionally, we may need to actually generate more addresses
                if (m_hdData->lastMainKey - ws.hdDerivationIndex < filter_hdUnusedToInclude + 50)
                    needMainAddresses = ExtraAddresses;
            }
        }
    }

    if (needChangeAddresses + needMainAddresses > 0) {
        deriveHDKeys(needMainAddresses, needChangeAddresses);
        updateBloom = false; // the deriveHDKeys just called rebuildBloom
        return true;
    }
    return false;
}

void Wallet::deriveHDKeys(int mainChain, int changeChain, uint32_t startHeight)
{
    // mutex already locked
    assert(mainChain + changeChain > 0);
    while (changeChain + mainChain > 0) {
        WalletSecret secret;
        secret.initialHeight = startHeight;
        secret.fromHdWallet = true;
        const auto count = m_hdData->derivationPath.size();
        assert(count >= 2);
        if (mainChain > 0) {
            --mainChain;
            secret.hdDerivationIndex = ++m_hdData->lastMainKey;
            m_hdData->derivationPath[count - 2] = 0;
            logDebug(LOG_WALLET) << "Creating new private key. Derivation: 0," << secret.hdDerivationIndex;
        } else {
            assert(changeChain > 0);
            --changeChain;
            secret.fromChangeChain = true;
            secret.hdDerivationIndex = ++m_hdData->lastChangeKey;
            logDebug(LOG_WALLET) << "Creating new private key for change chain:" << secret.hdDerivationIndex;
            m_hdData->derivationPath[count - 2] = 1;
        }
        m_hdData->derivationPath[count - 1] = secret.hdDerivationIndex;
        if (isDecrypted()) {
            secret.privKey = m_hdData->masterKey.derive(m_hdData->derivationPath);
            assert(secret.privKey.isValid());
            const PublicKey pubkey = secret.privKey.getPubKey();
            secret.address = pubkey.getKeyId();
        }
        else { // the wallet is locked / encrypted, lets only use the public key for now
            const PublicKey pubkey = m_hdData->masterPubkey.derive(m_hdData->derivationPath);
            secret.address = pubkey.getKeyId();
        }

        m_walletSecrets.insert(std::make_pair(m_nextWalletSecretId++, secret));
        m_secretsChanged = true;
        m_utxoDirty = true;
    }

    rebuildBloom();
    saveSecrets();
}

bool Wallet::walletIsImporting() const
{
    return m_walletIsImporting;
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
        if (m_walletIsImporting) {
            /* While importing the peers have our bloom filter and as such we could get notifications of
             * transactions entering the mempool. Which would be messy as they really should be added
             * after all the imported ones.
             * So we simply reject them for now. The peers will get a 'mempool' call at the end of the import
             * to fetch any such transactions (again).
             */
            return;
        }
        firstNewTransaction = m_nextWalletTransactionId;
        const uint256 txid = tx.createHash();
        if (m_txidCache.find(txid) != m_txidCache.end()) // already known
            return;

        std::map<uint64_t, SignatureType> signatureTypes;
        WalletTransaction wtx = createWalletTransactionFromTx(tx, txid, signatureTypes, &notification);
        assert(wtx.isCoinbase == false);
        if (wtx.outputs.empty() && wtx.inputToWTX.empty()) {
            // no connection to our UTXOs
            if (++m_bloomScore > 25)
                rebuildBloom(ForceBuild);
            return;
        }
        m_utxoDirty = true;
        setUserOwnedWallet(true);
        wtx.minedBlockHeight = Wallet::Unconfirmed;
        wtx.transactionTime = time(nullptr);
        bool dummy = false;
        while (updateHDSignatures(wtx, dummy)) {
            // if we added a bunch of new private keys, then rerun the matching
            // so we make sure we matched all we can
            auto newWtx = createWalletTransactionFromTx(tx, txid, signatureTypes, &notification);
            wtx.outputs = newWtx.outputs;
            wtx.userComment = newWtx.userComment;
            createdNewKeys = true;
        }

        // Remember the signature type used for specific private keys
        updateSignatureTypes(signatureTypes);

        // Mark as locked the UTXOs that this tx spent to avoid double spending them.
        for (auto i = wtx.inputToWTX.begin(); i != wtx.inputToWTX.end(); ++i) {
            m_lockedOutputs.insert(std::make_pair(i->second, m_nextWalletTransactionId));
        }

        // insert new UTXOs and update possible hits in the paymentRequests
        for (auto i = wtx.outputs.begin(); i != wtx.outputs.end(); ++i) {
            uint64_t key = m_nextWalletTransactionId;
            key <<= 16;
            key += i->first;
            logDebug(LOG_WALLET) << "   inserting output"<< i->first << Log::Hex << "TxIndex:" << i->second.walletSecretId << "outRef:" << key;
            m_unspentOutputs.insert(std::make_pair(key, i->second.value));
            // TODO somehow make payment requests know about this.
        }

        // and remember the transaction
        m_txidCache.insert(std::make_pair(wtx.txid, m_nextWalletTransactionId));
        m_walletTransactions.insert(std::make_pair(m_nextWalletTransactionId++, wtx));
        m_walletChanged = true;

        logCritical(LOG_WALLET) << "Wallet" << m_segment->segmentId() << "claims" << tx.createHash() << "[unconfirmed]";
    } // mutex scope
    saveTransaction(tx);
    recalculateBalance();
    emit startDelayedSave();

    emit utxosChanged();
    emit appendedTransactions(firstNewTransaction, 1);
    FloweePay::instance()->sendTransactionNotification(notification);
    if (createdNewKeys)
        rebuildBloom();
}

void Wallet::newTransactions(const uint256 &blockId, int blockHeight, const std::deque<Tx> &blockTransactions)
{
    auto transactions = WalletPriv::sortTransactions(blockTransactions);
    std::deque<Tx> transactionsToSave;
    bool needNewBloom = false;
    {
        QMutexLocker locker(&m_lock);
        /* We ask multiple peers for transactions, each hit gets a callback to newTransactions.
         * There may be nothing new. though. Most likely even. The next method just checks
         * the transaction IDs and if we already know all of them, we simply return.
         */
        if (anythingNew(blockHeight, blockTransactions) == false)
            return;
        std::set<int> ejectedTransactions;
        /*
         * Our UTXO based system requires interpretation of state based on explicit ordering.
         * So if we get something inserted out-of-order we're going to rollback the ones we already
         * have AFTER this and re-apply them later.
         */
        const auto insertBeforeData = removeTransactionsAfter(blockHeight - 1); // remove them for this block too!

        int firstNewTransaction = m_nextWalletTransactionId;
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
                    if (++m_bloomScore > 25)
                        needNewBloom = true;
                    continue;
                }
                if (wtx.isCashFusionTx) // improve behavior of bloom filters
                    m_walletStoresCashFusions = true;
            }
            else {
                // we already seen it before.
                wtx = m_walletTransactions.find(oldTx->second)->second;
                // check if the one we saw was unconfirmed or not.
                if (wtx.minedBlockHeight >= 0)
                    continue;
                walletTransactionId = oldTx->second;
            }
            m_utxoDirty = true;
            const bool wasUnconfirmed = wtx.minedBlockHeight == Wallet::Unconfirmed;
            while (updateHDSignatures(wtx, needNewBloom)) {
                // if we added a bunch of new private keys, then rerun the matching
                // so we make sure we matched all we can
                auto newWtx = createWalletTransactionFromTx(tx, txid, signatureTypes, &notification);
                wtx.outputs = newWtx.outputs;
                wtx.userComment = newWtx.userComment;
            }
            wtx.minedBlock = blockId;
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
                    logDebug(LOG_WALLET) << "   inserting output"<< i->first << Log::Hex << i->second.walletSecretId << key;
                    m_unspentOutputs.insert(std::make_pair(key, i->second.value));
                    if (!m_singleAddressWallet && i->second.value == 547) {
                        // special case so-called 'spam' outputs and instantly lock it for privacy reasons
                        // those outputs can only be spent by combining them with others, combining inputs
                        // is a potential loss of privacy (makes keys tracable) that is not worth the benefit
                        // of this low value output.
                        m_lockedOutputs.insert(std::make_pair(key, -1)); // -1 is our reason
                    }
                }
                if (wtx.isCoinbase) {
                    // a coinbase can not be spent until its is under a list of blocks of MATURATION_AGE
                    // as such we need to lock the output from being used.
                    m_lockedOutputs.insert(std::make_pair(key, (blockHeight + MATURATION_AGE) * -1));
                }

                // Check and update the private key data, if needed.
                const int privKeyId = i->second.walletSecretId;
                auto ws = m_walletSecrets.find(privKeyId);
                if (ws != m_walletSecrets.end() && ws->second.initialHeight == 0) {
                    // a private key has initialHeight set to zero if it comes from a HD seed,
                    // update this to the actual initial usage as we find a transaction matching it
                    ws->second.initialHeight = blockHeight;
                    m_secretsChanged = true;
                }

                // TODO somehow make payment requests know about this.
            }


            // and remember the transaction
            if (oldTx == m_txidCache.end()) {
                assert(walletTransactionId == m_nextWalletTransactionId);
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

            logCritical(LOG_WALLET) << "Wallet" << m_segment->segmentId() << "claims" << tx.createHash() << "@" << blockHeight;
            if (wasUnconfirmed)
                emit transactionConfirmed(walletTransactionId);
            if (notification.blockHeight > 0) {
                FloweePay::instance()->sendTransactionNotification(notification);
            }
        }
        assert(m_nextWalletTransactionId - firstNewTransaction == int(transactionsToSave.size()));

        // In processing the transactions from a block we might find that one of our
        // unconfirmed transactions ended up being rejected due to double spend.
        // We need to update our internal book keeping and eject the transaction.
        // We actually mark it Rejected (in the blockHeight).
        for (auto ejectedTx : ejectedTransactions) {
            auto tx = m_walletTransactions.find(ejectedTx);
            assert(tx != m_walletTransactions.end());
            logDebug(LOG_WALLET) << "Confirmed transaction(s) in block" << blockHeight <<
                          "made invalid transaction:" << ejectedTx << tx->second.txid;
            auto &wtx = tx->second;
            wtx.minedBlockHeight = Wallet::Rejected;
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

                // TODO somehow make payment requests know about this rejection.
            }
        }

        // the 'insert before' concept simply deletes and re-inserts, let
        // users know old transaction IDs no longer exist.
        for (auto id : insertBeforeData.oldTransactionIds) {
            emit transactionRemoved(id);
        }
        if (!transactionsToSave.empty()) {
            emit utxosChanged();
            emit appendedTransactions(firstNewTransaction, transactionsToSave.size());
        }

    } // mutex scope

    // outside the mutex do the file-IO heavy things like saving our newly found transactions.
    if (!transactionsToSave.empty()) {
        setUserOwnedWallet(true);
        for (const auto &tx : transactionsToSave) { // save the Tx to disk.
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
            auto pool = Streaming::pool(data.size() + AES_BLOCKSIZE);
            int size = crypto.encrypt(data.begin(), data.size(), pool->data());
            data = pool->commit(size);
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
        logFatal(LOG_WALLET) << "Could not store transaction" << e.what();
        throw;
    }
}

Tx Wallet::loadTransaction(const uint256 &txid, const std::shared_ptr<Streaming::BufferPool> &pool) const
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
        pool->reserve(txSize);
        reader.read(pool->begin(), txSize);
        if (m_encryptionLevel == FullyEncrypted) {
            // decrypt the tx
            assert(m_haveEncryptionKey); // checked above
            AES256CBCDecrypt crypto(&m_encryptionKey[0], &m_encryptionIR[0], true);
            auto encryptedTx = pool->commit(txSize);
            pool->reserve(txSize);
            txSize = crypto.decrypt(encryptedTx.begin(), txSize, pool->data());
        }
        return Tx(pool->commit(txSize));
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
        setTransactionComment(i->second, comment);
    }
    else {
        logCritical(LOG_WALLET) << "Comment added to not known transaction";
    }
}

void Wallet::setTransactionComment(int txIndex, const QString &comment)
{
    QMutexLocker locker(&m_lock);
    auto wtxIter = m_walletTransactions.find(txIndex);
    if (wtxIter == m_walletTransactions.end())
        return;
    auto &wtx = wtxIter->second;
    if (wtx.userComment != comment) {
        wtx.userComment = comment;
        m_walletChanged = true;

        emit transactionChanged(wtxIter->first);
        emit startDelayedSave();
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

int Wallet::findPrivKeyId(const KeyId &address) const
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
        return QString::fromUtf8(m_hdData->walletMnemonic.data(), m_hdData->walletMnemonic.size());
    return QString();
}

bool Wallet::isElectrumMnemonic() const
{
    if (m_hdData.get())
        return m_hdData->mnemonicFormat == HDMasterKey::ElectrumMnemonic;
    return false;
}

QString Wallet::hdWalletMnemonicPwd() const
{
    if (m_hdData.get())
        return QString::fromUtf8(m_hdData->walletMnemonicPwd.data(), m_hdData->walletMnemonicPwd.size());
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

QString Wallet::xpub() const
{
    if (m_hdData.get())
        return QString::fromStdString(m_hdData->masterPubkey.toString());

    return QString();
}

void Wallet::createHDMasterKey(const QString &mnemonic, const QString &pwd, const std::vector<uint32_t> &derivationPath, uint32_t startHeight, bool electrumFormat)
{
    assert(m_hdData.get() == nullptr);
    if (m_hdData.get()) {
        logFatal(LOG_WALLET) << "Refusing to replace HDMasterkey with new one";
        return;
    }
    // we only convert here in order to be 100% certain that the input from the user is
    // encoded properly, regardless of the underlying OS settings.

    // Explicitly use utf8 encoding.
    const auto mnemonicBytes = mnemonic.toUtf8();
    const auto pwdBytes = pwd.toUtf8();
    // convert to constBuf containers.
    auto pool = Streaming::pool(mnemonicBytes.size() + pwdBytes.size());
    pool->write(mnemonicBytes.constData(), mnemonicBytes.size());
    auto mnemonicBuf = pool->commit();
    pool->write(pwdBytes.constData(), pwdBytes.size());
    auto pwdBuf = pool->commit();

    m_hdData.reset(new HierarchicallyDeterministicWalletData(mnemonicBuf, derivationPath, pwdBuf,
                                                             electrumFormat ? HDMasterKey::ElectrumMnemonic
                                                                            : HDMasterKey::BIP39Mnemonic));
    // append two random numbers, to make clear the full length
    m_hdData->derivationPath.push_back(0);
    m_hdData->derivationPath.push_back(0);

    // Wallets add to the derivation path 2 items, the first is typically used
    // to indicate 0=receive-keys, 1=change-keys
    // So to reuse the derivationPath we store the last-used-index elsewhere.
    m_hdData->lastMainKey = -1;
    m_hdData->lastChangeKey = -1;
    QMutexLocker locker(&m_lock);
    if (startHeight < 1000000) { // when its a blockheight and not a timestamp.
        m_segment->blockSynched(startHeight);
        m_segment->blockSynched(startHeight); // yes, twice
        m_walletIsImporting = true;
    }
    deriveHDKeys(200, 200, startHeight);
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
    m_walletChanged = true;
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
    m_walletChanged = true;
    return true;
}

bool Wallet::isLocked(OutputRef outputRef) const
{
    QMutexLocker locker(&m_lock);
    return m_lockedOutputs.find(outputRef.encoded()) != m_lockedOutputs.end();
}

int Wallet::walletCreatedHeight() const
{
    /*
     * Wallets know at which heigh of the chain they were created, which is nice
     * because they never need to look at history on their addresses before that
     * blockheight.
     *
     * On Pay startup a wallet may be created before we actually saw the tip of
     * the headers-chain, meaning that the initial height is impossible to set.
     * In that small time-window the wallet will get its initial height set to
     * the current (unix) time,which is corrected to an actual block-height the
     * moment the headers are fully synched. (see headerSyncComplete())
     * In here, we simply return -1 in that case.
     */
    if (m_walletSecrets.empty() || m_walletSecrets.begin()->second.initialHeight >= 10000000)
        return -1;
    return m_walletSecrets.begin()->second.initialHeight;
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

int Wallet::findSecretFor(const Streaming::ConstBuffer &outputScript, bool &isCashToken) const
{
    auto theScript(outputScript);
    isCashToken = false;
    constexpr int MinTokenSize = /* prefix */ 1 + /* category */ + 32 + /* flags */ 1;
    if (outputScript.size() > MinTokenSize
        && static_cast<uint8_t>(outputScript.begin()[0]) == 0xef) {
        /*
         * This output holds a cash-token.
         * We eat the bytes that hold the cashtoken data and continue identifying the outputScript
         */
        const uint8_t* tokenData = reinterpret_cast<const uint8_t*>(outputScript.begin());
        uint8_t flags = tokenData[33];
        int skipAmount = MinTokenSize;
        if ((flags & 0x40) == 0x40) // has-commitment-length bit is set.
            skipAmount += tokenData[skipAmount++];
        if ((flags & 0x10) == 0x10) { // has-amount bit is set.
            const uint8_t amount = tokenData[skipAmount++];
            if (amount == 253) // compact-size design.
                skipAmount += 2;
            else if (amount == 254)
                skipAmount += 4;
            else if (amount == 255)
                skipAmount += 8;
        }

        isCashToken = true;
        if (outputScript.size() <= skipAmount) {
            logWarning() << "Invalid token data";
            return -1;
        }
        theScript = outputScript.mid(skipAmount);
    }

    std::vector<std::vector<uint8_t> > vSolutions;
    Script::TxnOutType whichType;
    if (!Script::solver(theScript, whichType, vSolutions))
        return -1;

    KeyId keyID;
    switch (whichType)
    {
    case Script::TX_PUBKEY:
        keyID = PublicKey(vSolutions[0]).getKeyId();
        break;
    case Script::TX_PUBKEYHASH:
        keyID = KeyId(uint160(vSolutions[0]));
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

void Wallet::rebuildBloom(RebuildBloomOption option)
{
    int unusedToInclude = filter_unusedToInclude;
    int hdUnusedToInclude = filter_hdUnusedToInclude;
    int changeUnusedToIncludeBase = filter_changeUnusedToInclude;
    if (m_walletStoresCashFusions) {
        // a wallet that uses fusions eats through change addresses like
        // cheap candy, let's avoid the need to upload it too often
        changeUnusedToIncludeBase += 50;
    }
    int changeUnusedToInclude = changeUnusedToIncludeBase;

    // on wallet creation we may not have yet synced the entire header chain,
    // in that case the secrets are given a height that is in seconds, in order
    // to find the proper blockheight matching it later.
    // As such, the presence of such initialHeights indicates the p2p net is not
    // yet at the tip of the headerchain, which makes sending out a bloom filter irrelevant.
    for (const auto &i : m_walletSecrets) {
        if (i.second.initialHeight >= 10000000) {
            logDebug(LOG_WALLET) << " not building bloom, still have date based private keys";
            return;
        }
    }
    if (option != ForceBuild && !m_utxoDirty)
        return;
    m_utxoDirty = false;

    auto lock = m_segment->clearFilter();
    //    ^ notice that the 'lock' we get back will broadcast the new filter when it goes out of scope.
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
        assert(i != m_walletTransactions.end());
        const auto &wtx = i->second;
        if (i->second.minedBlockHeight > 1) {
            for (const auto &output : wtx.outputs) {
                secretsWithBalance.insert(output.second.walletSecretId);
            }
        }
    }
    logDebug(LOG_WALLET) << "Rebuilding bloom filter." << m_segment->segmentId() << "UTXO-size:" << secretsWithBalance.size();
    for (auto &i : m_walletSecrets) {
        const auto &secret = i.second;
        assert(secret.initialHeight < 10000000); // see similar check at start of this method
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
                    /*
                     * The amount of unused change addresses we include is meant to be a
                     * 'gap'. Which means, after the last used addresss we should add that
                     * amount of addresses.
                     * This restoring of count is needed for some wallets that repeatedly have
                     * gaps between change addresses.
                     */
                    changeUnusedToInclude = changeUnusedToIncludeBase;

                    /*
                     * Ok, the above is true and is good for privacy. We want to avoid uploading stuff
                     * we don't ever expect money on.
                     * BUT, some pretty big wallet out there are not so nice and are known to reuse
                     * a change suddenly.
                     * Sooooo! We still actually include those 'old' keys during an import and
                     * convince owners of such wallets to switch to our wallet by being great!
                     */
                    if (m_walletIsImporting == false)
                        continue;
                }
                m_hdData->lastIncludedChangeKey = std::max(m_hdData->lastIncludedChangeKey, secret.hdDerivationIndex);
            }
            // HD from main chain. If we never saw this, include a limited set.
            else {
                if (secretsUsed.find(i.first) == secretsUsed.end()) {
                    if (--hdUnusedToInclude < 0)
                        continue;
                }
                else {
                    // Didn't reach the gap yet.
                    hdUnusedToInclude = filter_hdUnusedToInclude;
                }
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

bool Wallet::anythingNew(int blockHeight, const std::deque<Tx> &transactions) const
{
    for (const auto &tx : transactions) {
        auto id = tx.createHash();
        auto iter = m_txidCache.find(id);
        if (iter == m_txidCache.end())
            return true;
        auto txData = m_walletTransactions.find(iter->second);
        if (txData->second.minedBlockHeight != blockHeight)
            return true;
    }
    return false;
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
            // delete the TxInfoObject first, stopping broadcasts.
            m_broadcastingTransactions.removeAt(i);
            break;
         }
    }

    if (!success) {
        auto wtx = m_walletTransactions.find(txIndex);
        if (wtx != m_walletTransactions.end()) {
            logCritical(LOG_WALLET) << "Marking transaction invalid";
            auto &tx = wtx->second;
            if (tx.minedBlockHeight == Wallet::Unconfirmed) {
                // a transaction that has been added before, but now marked
                // rejected means we should revert some stuff that newTransaction() did.
                // - locked output
                // - utxo
                // - balance

                auto j = m_lockedOutputs.begin();
                while (j != m_lockedOutputs.end()) {
                    if (j->second == txIndex)
                        j = m_lockedOutputs.erase(j);
                    else
                        ++j;
                }
                for (auto out = tx.outputs.begin(); out != tx.outputs.end(); ++out) {
                    auto utxo = m_unspentOutputs.find(OutputRef(txIndex, out->first).encoded());
                    assert(utxo != m_unspentOutputs.end());
                    m_unspentOutputs.erase(utxo);
                }

                recalculateBalance();
            }
            else if (tx.minedBlockHeight > 0) {
                assert(false); // Can't imagine the usecase, so if this hits in a debug build lets fail-fast
                logWarning(LOG_WALLET) << "Transaction marked rejected that had blockHeight:" << tx.minedBlockHeight;
            }
            tx.minedBlockHeight = Wallet::Rejected;
            emit transactionChanged(txIndex); // notify the UI
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
    m_walletNameChanged = true;
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

int64_t Wallet::transactionTime(int txIndex) const
{
    QMutexLocker locker(&m_lock);
    auto iter = m_walletTransactions.find(txIndex);
    if (m_walletTransactions.end() == iter)
        throw std::runtime_error("Invalid tx-index");
    return iter->second.transactionTime;
}

int Wallet::transactionMined(int txIndex) const
{
    QMutexLocker locker(&m_lock);
    auto iter = m_walletTransactions.find(txIndex);
    if (m_walletTransactions.end() == iter)
        throw std::runtime_error("Invalid tx-index");
    return iter->second.minedBlockHeight;
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

KeyId Wallet::nextUnusedAddress()
{
    KeyId answer;
    reserveUnusedAddress(answer);
    return answer;
}

KeyId Wallet::nextUnusedChangeAddress()
{
    KeyId answer;
    reserveUnusedAddress(answer, ChangePath);
    return answer;
}

int Wallet::reserveUnusedAddress(KeyId &keyId, PrivKeyType pkt)
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

        // check if this address has already been sent and we can avoid the bloom rebuild
        if (m_hdData == nullptr || m_hdData->lastIncludedMainKey < i->first)
            rebuildBloom(ForceBuild); // make sure that we actually observe changes on this address
        return i->first;
    }

    // no unused addresses, lets make some.
    if (m_hdData.get()) {
        deriveHDKeys(100, 0);
        return reserveUnusedAddress(keyId);
    }

    const bool walletUnavailable = m_encryptionLevel > NotEncrypted && !isDecrypted();
    assert(!walletUnavailable);
    if (walletUnavailable)
        return 0;

    int answer = m_nextWalletSecretId;
    for (int i = 0; i < 100; ++i) {
        WalletSecret secret;
        secret.privKey.makeNewKey();
        const PublicKey pubkey = secret.privKey.getPubKey();
        secret.address = pubkey.getKeyId();
        if (i == 0)
            keyId = secret.address;
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
    if (m_encryptionLevel == FullyEncrypted && !isDecrypted()) {
        // this protects against race conditions where the SPV action
        // lags behind a wallet that gets closed.
        return;
    }
    m_walletChanged = true;
    m_lastBlockHeightSeen = height;
    emit lastBlockSynchedChanged();

    bool oneMatured = false;
    auto i = m_lockedOutputs.begin();
    while (i != m_lockedOutputs.end()) {
        if (i->second < -1) {
            const auto matureHeight = i->second * -1;
            if (matureHeight <= height) {
                oneMatured = true;
                i = m_lockedOutputs.erase(i);
                continue;
            }
        }
        ++i;
    }
    if (oneMatured) {
        // when an immature coinbase became spendable, this updates the balance
        recalculateBalance();
        emit utxosChanged();
    }

    if (height == FloweePay::instance()->headerChainHeight()) {
        m_walletIsImporting = false;
        // start this in my own thread and free of mutex-locks
        QTimer::singleShot(0, this, SLOT(broadcastUnconfirmed()));
    }
}

// called every app-start we reach the tip
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

    const Blockchain &blockchain = FloweePay::instance()->p2pNet()->blockchain();
    bool changedOne = false;
    for (auto iter = m_walletSecrets.begin(); iter != m_walletSecrets.end(); ++iter) {
        if (iter->second.initialHeight > 10000000) {
            // this is a time based height, lets resolve it to a real height.
            iter->second.initialHeight = blockchain.blockHeightAtTime(iter->second.initialHeight) - 1;
            m_secretsChanged = true;
            changedOne = true;
        }
    }
    if (changedOne) {
        // broadcast to peers our bloom filter, which would have skipped all
        // time-based blocks before.
        rebuildBloom(ForceBuild);
        // make the wallet initial sync also show something sane.
        if (m_segment)
            m_segment->blockSynched(blockchain.height());
    }
}

// called on startup to ensure we don't get stuck waiting for tip.
void Wallet::checkHeaderSyncComplete(const Blockchain &blockchain)
{
    if (m_walletSecrets.empty())
        return;
    auto timestamp = m_walletSecrets.begin()->second.initialHeight;
    if (timestamp > 10000000) {
        // is a time based height
        auto block = blockchain.block(blockchain.height());
        if (block.nTime >= timestamp)
            headerSyncComplete();
    }
}

void Wallet::broadcastUnconfirmed()
{
    assert(thread() == QThread::currentThread());

    // we are (again) up-to-date.
    // Lets broadcast any transactions that have not yet been confirmed.
    QMutexLocker locker(&m_lock);
    m_broadcastingTransactions.clear();

    for (auto iter = m_walletTransactions.begin();
         iter != m_walletTransactions.end(); ++iter) {

        if (iter->second.minedBlockHeight == Wallet::Unconfirmed) {
            auto tx = loadTransaction(iter->second.txid, Streaming::pool(0));
            if (tx.data().size() > 64) {
                auto bc = std::make_shared<TxInfoObject>(this, iter->first, tx);
                bc->moveToThread(thread());
                logDebug(LOG_WALLET) << "  broadcasting transaction" << tx.createHash() << tx.size();
                m_broadcastingTransactions.append(bc);
                FloweePay::instance()->p2pNet()->connectionManager().broadcastTransaction(bc);
            }
            else {
                logCritical(LOG_WALLET) << "Unconfirmed transaction could not be found on disk!";
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
    const PublicKey pubkey = secret.privKey.getPubKey();
    secret.address = pubkey.getKeyId();
    secret.initialHeight = currentBlockheight;
    m_walletSecrets.insert(std::make_pair(m_nextWalletSecretId++, secret));
    m_secretsChanged = true;
    saveSecrets();

    if (currentBlockheight < 10000000) {
        // if that is false, its a timestamp: headers are not yet synched)
        rebuildBloom(ForceBuild);
    }
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

        const PublicKey pubkey = secret.privKey.getPubKey();
        secret.address = pubkey.getKeyId();
        secret.initialHeight = startBlockHeight;
        m_walletSecrets.insert(std::make_pair(m_nextWalletSecretId++, secret));
        m_secretsChanged = true;
        saveSecrets();

        if (startBlockHeight < 10000000) {
            // if that is false, its a timestamp: headers are not yet synched)
            rebuildBloom(ForceBuild);
            m_walletIsImporting = true;
        }
        return true;
    }
    logFatal(LOG_WALLET) << "ERROR. Wallet: added string is not a private key";
    return false;
}

Streaming::ConstBuffer Wallet::readSecrets() const
{
    std::ifstream in((m_basedir / "secrets.dat").string());
    if (!in.is_open())
        throw std::runtime_error("Missing secrets.dat");
    const auto dataSize = boost::filesystem::file_size(m_basedir / "secrets.dat");
    auto pool = Streaming::pool(dataSize);
    in.read(pool->begin(), dataSize);
    return pool->commit(dataSize);
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
                logCritical(LOG_WALLET) << "Reading (encrypted) secrets file failed";
                return;
            }
            fileData = pool.commit(newSize);
        }
    }
    Streaming::MessageParser parser(fileData);
    WalletSecret secret;
    std::string xpub;
    Streaming::ConstBuffer mnemonic, mnemonicPwd;
    std::vector<char> encryptedMnemonic, encryptedMnemonicPwd;
    std::vector<uint32_t> derivationPath;
    int derivationPathChangeIndex = -1;
    int derivationPathMainIndex = -1;
    int index = 0;
    HDMasterKey::MnemonicType mnemonicFormat = HDMasterKey::BIP39Mnemonic;
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
                logCritical(LOG_WALLET) << "Loaded" << index << ad;
#endif
            }
            secret = WalletSecret();
        }
        else if (parser.tag() == WalletPriv::Index) {
            index = parser.intData();
        }
        else if (parser.tag() == WalletPriv::CryptoChecksum) {
            assert(parser.intData() >= 0);
            assert(parser.intData() <= 0xFFFF);
            m_encryptionChecksum = parser.intData();
        }
        else if (parser.tag() == WalletPriv::PrivKey) {
            auto d = parser.unsignedBytesData();
            secret.privKey.set(d.begin(), d.end(), true);
        }
        else if (parser.tag() == WalletPriv::PrivKeyEncrypted) {
            secret.encryptedPrivKey = parser.bytesData();
            m_encryptionLevel = SecretsEncrypted;
        }
        else if (parser.tag() == WalletPriv::PubKeyHash) {
            auto d = parser.bytesDataBuffer();
            secret.address = KeyId(d.begin());
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
            mnemonic = parser.bytesDataBuffer();
        }
        else if (parser.tag() == WalletPriv::HDWalletMnemonicEncrypted) {
            encryptedMnemonic = parser.bytesData();
            m_encryptionLevel = SecretsEncrypted;
        }
        else if (parser.tag() == WalletPriv::HDWalletMnemonicPassword) {
            mnemonicPwd = parser.bytesDataBuffer();
        }
        else if (parser.tag() == WalletPriv::HDWalletMnemonicFormat) {
            static_assert(HDMasterKey::BIP39Mnemonic == 0);
            static_assert(HDMasterKey::ElectrumMnemonic == 1);
            mnemonicFormat = static_cast<HDMasterKey::MnemonicType>(parser.intData());
        }
        else if (parser.tag() == WalletPriv::HDWalletMnemonicPasswordEncrypted) {
            encryptedMnemonicPwd = parser.bytesData();
            m_encryptionLevel = SecretsEncrypted;
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
        else if (parser.tag() == WalletPriv::HDXPub) {
            xpub = parser.stringData();
        }
        else if (parser.tag() == WalletPriv::WalletVersion) {
            m_walletVersion = parser.intData();
        }
    }

    assert(m_hdData.get() == nullptr);
    if (mnemonicFormat != HDMasterKey::BIP39Mnemonic && mnemonicFormat != HDMasterKey::ElectrumMnemonic)
        logFatal(LOG_WALLET) << "Found unknown mnemonic type for HD wallet:" << int(mnemonicFormat);
    if ((xpub.empty() && mnemonic.isEmpty()) != derivationPath.empty())
        logFatal(LOG_WALLET) << "Found incomplete data for HD wallet";
    else if (!mnemonic.isEmpty())
        m_hdData.reset(new HierarchicallyDeterministicWalletData(mnemonic, derivationPath, mnemonicPwd, mnemonicFormat));
    else if (!xpub.empty())
        m_hdData.reset(new HierarchicallyDeterministicWalletData(xpub, derivationPath));
    if (m_hdData) {
        m_hdData->lastChangeKey = derivationPathChangeIndex;
        m_hdData->lastMainKey = derivationPathMainIndex;
        m_hdData->encryptedWalletMnemonic = encryptedMnemonic;
        m_hdData->encryptedWalletMnemonicPwd = encryptedMnemonicPwd;
        m_hdData->mnemonicFormat = mnemonicFormat;
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
        // Reserve enough space. Notice that string-length and encoded length may be different.
        hdDataSize += m_hdData->walletMnemonic.size() * 5;
        hdDataSize += m_hdData->walletMnemonicPwd.size() * 5;
        hdDataSize += m_hdData->encryptedWalletMnemonicPwd.size();
        hdDataSize += m_hdData->encryptedWalletMnemonic.size();
        hdDataSize += m_hdData->derivationPath.size() * 6;
        hdDataSize += 104; // for the xpub, lastChangeIndex, lastReceiveIndex, mnemonicFormat
    }

    Streaming::MessageBuilder builder(Streaming::pool(100 + m_walletSecrets.size() * 90 + hdDataSize));
    if (m_encryptionLevel >= SecretsEncrypted) {
        // Please note that this field HAS to be the first one.
        builder.add(WalletPriv::CryptoChecksum, (uint64_t) m_encryptionChecksum);
    }
    builder.add(WalletPriv::WalletVersion, m_walletVersion);
    if (m_hdData.get()) {
        if (m_encryptionLevel == SecretsEncrypted) {
            // Save encrypted mnemonic and 'password' here, with the unencrypted xpub.
            assert(!m_hdData->encryptedWalletMnemonic.empty());
            builder.add(WalletPriv::HDWalletMnemonicEncrypted, m_hdData->encryptedWalletMnemonic);
            if (!m_hdData->encryptedWalletMnemonicPwd.empty())
                builder.add(WalletPriv::HDWalletMnemonicPasswordEncrypted, m_hdData->encryptedWalletMnemonicPwd);
            builder.add(WalletPriv::HDXPub, m_hdData->masterPubkey.toString());
        }
        else {
            builder.add(WalletPriv::HDWalletMnemonic, std::string(m_hdData->walletMnemonic.data(), m_hdData->walletMnemonic.size()));
            if (!m_hdData->walletMnemonicPwd.empty())
                builder.add(WalletPriv::HDWalletMnemonicPassword,
                                     std::string(m_hdData->walletMnemonicPwd.data(), m_hdData->walletMnemonicPwd.size()));
        }
        for (const uint32_t item : m_hdData->derivationPath) {
            builder.add(WalletPriv::HDWalletPathItem,  static_cast<uint64_t>(item));
        }
        if (m_hdData->lastChangeKey >= 0)
            builder.add(WalletPriv::HDWalletLastChangeIndex,  m_hdData->lastChangeKey);
        if (m_hdData->lastMainKey >= 0)
            builder.add(WalletPriv::HDWalletLastReceiveIndex,  m_hdData->lastMainKey);
        builder.add(WalletPriv::HDWalletMnemonicFormat, static_cast<int>(m_hdData->mnemonicFormat));
    }
    for (const auto &item : m_walletSecrets) {
        const auto &secret = item.second;
        builder.add(WalletPriv::Index, item.first);
        // In HD wallets the secret key may be missing totally in case this secret was auto-created
        // on a SecretsEncrypted wallet (from the xpub).
        // Be sure to only save what we have.
        if (!secret.encryptedPrivKey.empty())
            builder.add(WalletPriv::PrivKeyEncrypted, secret.encryptedPrivKey);
        if (secret.privKey.isValid() && m_encryptionLevel != SecretsEncrypted)
            builder.addByteArray(WalletPriv::PrivKey, secret.privKey.begin(), secret.privKey.size());
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
        std::filesystem::create_directories(m_basedir.string());

        if (m_encryptionLevel == FullyEncrypted) {
            auto pool = Streaming::pool(data.size() + AES_BLOCKSIZE);
            AES256CBCEncrypt crypto(&m_encryptionKey[0], &m_encryptionIR[0], true);
            int size = crypto.encrypt(data.begin(), data.size(), pool->data());
            data = pool->commit(size);
        }
        const auto basefile = (m_basedir / "secrets.dat").string();
        std::ofstream outFile(basefile + "~");
        outFile.write(data.begin(), data.size());
        outFile.flush();
        outFile.close();
        std::filesystem::rename(basefile + "~", basefile);
    } catch (const std::exception &e) {
        logFatal(LOG_WALLET) << "Failed to save the database. Reason:" << e.what();
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
    if (!in.is_open()) {
        m_walletIsImporting = true; // If we have no history, we need to rebuild it
        return;
    }
    QMutexLocker locker(&m_lock);
    auto dataSize = boost::filesystem::file_size(m_basedir / "wallet.dat");
    Streaming::BufferPool pool(dataSize);
    in.read(pool.data(), dataSize);
    if (m_encryptionLevel == FullyEncrypted) {
        if (!m_haveEncryptionKey)
            return;

        // we decrypt it.
        auto data = pool.commit(dataSize);
        AES256CBCDecrypt crypto(&m_encryptionKey[0], &m_encryptionIR[0], true);
        pool.reserve(dataSize);
        dataSize = crypto.decrypt(data.begin(), data.size(), pool.data());
        if (dataSize == 0) {
            logCritical(LOG_WALLET) << "Reading (encrypted) wallet.dat file failed";
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
            logFatal(LOG_WALLET) << "Wallet has tx: " << wtx.txid << "@" << wtx.minedBlockHeight;
            for (auto pair : wtx.outputs) {
                logFatal(LOG_WALLET) << "  ++ " << pair.first << pair.second.value << "sat";
            }
            for (auto pair : wtx.inputToWTX) {
                OutputRef ref(pair.second);
                logFatal(LOG_WALLET) << "  -- " << pair.first << ref.txIndex() << ref.outputIndex();
                auto w = m_walletTransactions.find(ref.txIndex());
                if (w != m_walletTransactions.end())
                    logFatal(LOG_WALLET) << "     " << w->second.txid;
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
            if (wtx.isCashFusionTx)
                m_walletStoresCashFusions = true;
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
        else if (parser.tag() == WalletPriv::OutputHoldsToken) {
            output.holdsCashToken = parser.boolData();
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
            auto walletName = QString::fromUtf8(parser.stringData().c_str(), parser.dataLength());
            if (m_name != walletName) {
                m_name = walletName;
                m_walletNameChanged = true;
            }
        }
        else if (parser.tag() == WalletPriv::UserComment) {
            assert(parser.isString());
            wtx.userComment = QString::fromUtf8(parser.stringData().c_str(), parser.dataLength());
        }
        else if (parser.tag() == WalletPriv::TransactionTime) {
            wtx.transactionTime = parser.longData();
        }
        else if (parser.tag() == WalletPriv::LastSynchedBlock) {
            highestBlockHeight = std::max(parser.intData(), highestBlockHeight);
        }
        else if (parser.tag() == WalletPriv::WalletIsImporting) {
             m_walletIsImporting = parser.boolData();
        }
    }

    // after inserting all outputs during load, now remove all inputs these tx's spent.
    for (auto txIndex : newTx) {
        auto iter = m_walletTransactions.find(txIndex);
        assert(iter != m_walletTransactions.end());

        if (iter->second.minedBlockHeight == Wallet::Rejected)
            continue;
        if (iter->second.minedBlockHeight != Wallet::Unconfirmed) {
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
    }
    recalculateBalance();

#ifdef DEBUGUTXO
    for (auto outputIter : m_unspentOutputs) {
        OutputRef ref(outputIter.first);
        auto utxo = m_walletTransactions.find(ref.txIndex());
        assert(utxo != m_walletTransactions.end());
        auto out = utxo->second.outputs.find(ref.outputIndex());
        assert(out != utxo->second.outputs.end());
        assert(out->second.value == outputIter.second);
        logFatal(LOG_WALLET) << "Unspent: " << utxo->second.txid << ref.outputIndex() << "\t->" << out->second.value << "sats";

        auto locked = m_lockedOutputs.find(ref.encoded());
        if (locked != m_lockedOutputs.end()) {
            logFatal(LOG_WALLET) << "      \\=  Locked UTXO" << locked->second;
        }
    }
#endif

#ifndef NDEBUG
    // sanity check: the inputs should resolve to transactions in our list.
    for (const auto &tx : m_walletTransactions) {
        for (auto input : tx.second.inputToWTX) {
            auto key = input.second;
            const int outIndex = key & 0xFFFF;
            key >>= 16;
            assert(m_walletTransactions.find(key) != m_walletTransactions.end());
            auto spendingTx = m_walletTransactions.at(key);
            assert(spendingTx.outputs.find(outIndex) != spendingTx.outputs.end());
        }
    }

    // check sanity of autoLockedOutputs struct
    for (auto &pair : m_lockedOutputs) {
        auto utxoLink = m_unspentOutputs.find(pair.first);
        assert(utxoLink != m_unspentOutputs.end());
        // zero means its user-locked, -1 is spam-locked
        // -101 and further is a coinbase maturation height, times -1
        if (pair.second > 0) {
            auto w = m_walletTransactions.find(pair.second);
            assert(w != m_walletTransactions.end());
        }
        if (pair.second < -1) { // is coinbase lock
            assert(pair.second < -1 * MATURATION_AGE);
            const OutputRef ref(pair.first);
            auto w = m_walletTransactions.find(ref.txIndex());
            assert(w != m_walletTransactions.end());
            assert(w->second.isCoinbase);
        }
    }
#endif
    m_walletChanged = false;
    // correct for bad input in wallet file
    for (auto i = m_lockedOutputs.begin(); i != m_lockedOutputs.end();) {
        auto utxoIter = m_unspentOutputs.find(i->first);
        if (utxoIter == m_unspentOutputs.end()) {
            logCritical(LOG_WALLET) << "Found faulty 'locked' output-ref, dropping";
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
    if (m_walletNameChanged) {
        try {
            std::filesystem::create_directories(m_basedir.string());
            const auto basefile = (m_basedir / "name").string();
            std::ofstream outFile(basefile + "~");
            const auto nameBytes = m_name.toUtf8();
            outFile.write(nameBytes.constBegin(), nameBytes.size());
            outFile.flush();
            outFile.close();
            std::filesystem::rename(basefile + "~", basefile);
            m_walletNameChanged = false;
        } catch (const std::exception &e) {
            logFatal(LOG_WALLET) << "Failed to save the wallet-name. Reason:" << e;
        }
    }
    if (!m_walletChanged)
        return;

    if (m_encryptionLevel == FullyEncrypted && !m_haveEncryptionKey) {
        logFatal(LOG_WALLET) << "Wallet" << m_name << (m_walletChanged ? "was changed" : "has changed payment requests")
                   << "We can't save it since its fully encrypted and not open right now";
        throw std::runtime_error("Can not save a pin to open wallet that is closed.");
    }

    int saveFileSize = 100;
    for (const auto &i : m_walletTransactions) {
        const auto &wtx = i.second;
        saveFileSize += 110 + wtx.inputToWTX.size() * 22 + wtx.outputs.size() * 30;
    }

    auto pool = std::make_shared<Streaming::BufferPool>(saveFileSize);
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
            if (i->second.holdsCashToken)
                builder.add(WalletPriv::OutputHoldsToken, true);
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
        if (item.second.transactionTime != 0)
            builder.add(WalletPriv::TransactionTime, static_cast<uint64_t>(item.second.transactionTime));
        builder.add(WalletPriv::Separator, true);
    }
    builder.add(WalletPriv::LastSynchedBlock, m_segment->lastBlockSynched());
    if (m_walletIsImporting)
        builder.add(WalletPriv::WalletIsImporting, true);

    auto data = builder.buffer();
    try {
        std::filesystem::create_directories(m_basedir.string());
        if (m_encryptionLevel == FullyEncrypted) {
            AES256CBCEncrypt crypto(&m_encryptionKey[0], &m_encryptionIR[0], true);
            pool->reserve(data.size() + AES_BLOCKSIZE);
            int size = crypto.encrypt(data.begin(), data.size(), pool->data());
            data = pool->commit(size);
        }

        const auto basefile = (m_basedir / "wallet.dat").string();
        std::ofstream outFile(basefile + "~");
        outFile.write(data.begin(), data.size());
        outFile.flush();
        outFile.close();
        std::filesystem::rename(basefile + "~", basefile);
    } catch (const std::exception &e) {
        logFatal(LOG_WALLET) << "Failed to save the wallet.dat. Reason:" << e;
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
        assert(wtx != m_walletTransactions.end());
        const int h = wtx->second.minedBlockHeight;
        if (h == Wallet::Rejected)
            continue;
        const auto loi = m_lockedOutputs.find(utxo.first);
        if (loi != m_lockedOutputs.end()) {
            // locked utxos are the result of already sent but not yet mined transactions
            // but also when the user goes and locks one manually.
            if (loi->second < -1) { // locked coinbase where value is (the negative) maturation height
                balanceImmature += utxo.second;
                continue;
            }
            else if (loi->second != 0) { // zero means user-locked.
                continue;
            }
        }
        if (h == Wallet::Unconfirmed)
            balanceUnconfirmed += utxo.second;
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
