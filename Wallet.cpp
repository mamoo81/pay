/*
 * This file is part of the Flowee project
 * Copyright (C) 2020-2021 Tom Zander <tom@flowee.org>
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
#include "TransactionInfo.h"

#include <primitives/script.h>
#include <streaming/BufferPool.h>
#include <streaming/MessageBuilder.h>
#include <streaming/MessageParser.h>
#include <base58.h>

#include <QFile>
#include <QSet>
#include <QTimer>
#include <QThread>
#include <cashaddr.h>

// #define DEBUGUTXO

/*
 * Fee estimation needs to know how much space is used per input.
 * 32 + 4 for the prevTx
 * Then 72 + 33 bytes for the signature and pub-key (+-1).
 * Using schnorr we can gain 8 bytes for the signature (not included here).
 */
constexpr int BYTES_PER_OUTPUT = 149;
constexpr int MATURATION_AGE  = 100; // the amount of blocks a coinbase takes before we can spend it

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
    : m_walletChanged(true)
{
}

Wallet::Wallet(const boost::filesystem::path &basedir, uint16_t segmentId)
    : m_segment(new PrivacySegment(segmentId, this)),
      m_basedir(basedir / QString("wallet-%1/").arg(segmentId).toStdString())
{
    loadSecrets();
    loadWallet();
    rebuildBloom();
}

Wallet::~Wallet()
{
    // these return instantly if nothing has to be saved.
    saveSecrets();
    saveWallet();
}

Wallet::WalletTransaction Wallet::createWalletTransactionFromTx(const Tx &tx, const uint256 &txid) const
{
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
                auto i = m_txidCash.find(prevTxhash);
                prevTx.setTxIndex((i != m_txidCash.end()) ? i->second : 0);
                if (i != m_txidCash.end())
                    logDebug() << "  Input:" << inputIndex << "prevTx:" << prevTxhash
                               << Log::Hex << i->second << prevTx.encoded();
            }
        } else  if (iter.tag() == Tx::PrevTxIndex) {
            if (prevTx.txIndex() > 0) { // we know the prevTx
                assert(iter.longData() < 0xFFFF); // it would break our scheme
                prevTx.setOutputIndex(iter.intData());

                auto utxo = m_unspentOutputs.find(prevTx.encoded());
                if (utxo != m_unspentOutputs.end()) {
                    // input is spending one of our UTXOs
                    logDebug() << "   -> spent UTXO";
                    wtx.inputToWTX.insert(std::make_pair(inputIndex, prevTx.encoded()));
                }
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
            }
        }
    }
    return wtx;
}

void Wallet::newTransaction(const Tx &tx)
{
    int firstNewTransaction;
    {
        QMutexLocker locker(&m_lock);
        firstNewTransaction = m_nextWalletTransactionId;
        const uint256 txid = tx.createHash();
        if (m_txidCash.find(txid) != m_txidCash.end()) // already known
            return;

        WalletTransaction wtx = createWalletTransactionFromTx(tx, txid);
        Q_ASSERT(wtx.isCoinbase == false);
        if (wtx.outputs.empty() && wtx.inputToWTX.empty()) {
            // no connection to our UTXOs
            if (--m_bloomScore < 25)
                rebuildBloom();
            return;
        }
        wtx.minedBlockHeight = WalletPriv::Unconfirmed;

        // Mark UTXOs locked that this tx spent to avoid double spending them.
        for (auto i = wtx.inputToWTX.begin(); i != wtx.inputToWTX.end(); ++i) {
            m_autoLockedOutputs.insert(std::make_pair(i->second, m_nextWalletTransactionId));
        }

        // insert new UTXOs and update possible hits in the paymentRequests
        for (auto i = wtx.outputs.begin(); i != wtx.outputs.end(); ++i) {
            uint64_t key = m_nextWalletTransactionId;
            key <<= 16;
            key += i->first;
            logDebug() << "   inserting output"<< i->first << Log::Hex << i->second.walletSecretId << key;
            m_unspentOutputs.insert(std::make_pair(key, i->second.value));

            const int privKeyId = i->second.walletSecretId;
            for (auto pr : m_paymentRequests) {
                if (pr->m_privKeyId == privKeyId) {
                    pr->addPayment(key, i->second.value);
                    wtx.userComment = pr->message();
                }
            }
        }

        // and remember the transaction
        m_txidCash.insert(std::make_pair(wtx.txid, m_nextWalletTransactionId));
        m_walletTransactions.insert(std::make_pair(m_nextWalletTransactionId++, wtx));
        m_walletChanged = true;

        logCritical() << "Wallet" << m_segment->segmentId() << "claims" << tx.createHash() << "[unconfirmed]";
    } // mutex scope
    saveTransaction(tx);
    recalculateBalance();

    emit utxosChanged();
    emit appendedTransactions(firstNewTransaction, 1);
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
        for (auto &tx: transactions) {
            const uint256 txid = tx.createHash();
            WalletTransaction wtx;

            auto oldTx = m_txidCash.find(txid);
            int walletTransactionId = m_nextWalletTransactionId;
            if (oldTx == m_txidCash.end()) {
                wtx = createWalletTransactionFromTx(tx, txid);
                if (wtx.outputs.empty() && wtx.inputToWTX.empty()) {
                    // no connection to our UTXOs
                    if (--m_bloomScore < 25)
                        rebuildBloom();
                    continue;
                }
            } else {
                // we already seen it before.
                wtx = m_walletTransactions.find(oldTx->second)->second;
                // check if the one we saw was unconfirmed or not.
                if (wtx.minedBlockHeight >= 0)
                    continue;
                walletTransactionId = oldTx->second;
            }
            const bool wasUnconfirmed = wtx.minedBlockHeight == WalletPriv::Unconfirmed;
            wtx.minedBlock = header.createHash();
            wtx.minedBlockHeight = blockHeight;

            // remove UTXOs this Tx spent
            for (auto i = wtx.inputToWTX.begin(); i != wtx.inputToWTX.end(); ++i) {
                auto iter = m_unspentOutputs.find(i->second);
                assert(iter != m_unspentOutputs.end()); // a double spend? With checked merkle-block? Thats...odd.
                if (iter != m_unspentOutputs.end())
                    m_unspentOutputs.erase(iter);

                // unlock UTXOs
                auto lockedIter = m_autoLockedOutputs.find(i->second);
                if (lockedIter != m_autoLockedOutputs.end()) {
                    if (lockedIter->second != walletTransactionId) {
                        // if this output was locked by another transaction then that means
                        // that other transaction was rejected (double spent) since the
                        // tx we are now processing made it into a block.
                        // remember so we can process ejected transactions below.
                        ejectedTransactions.insert(lockedIter->second);
                    }
                    m_autoLockedOutputs.erase(lockedIter);
                }
            }

            // process new UTXOs
            for (auto i = wtx.outputs.begin(); i != wtx.outputs.end(); ++i) {
                uint64_t key = m_nextWalletTransactionId;
                key <<= 16;
                key += i->first;
                if (!wasUnconfirmed) { // unconfirmed transactions already had their outputs added
                    logDebug() << "   inserting output"<< i->first << Log::Hex << i->second.walletSecretId << key;
                    m_unspentOutputs.insert(std::make_pair(key, i->second.value));
                }

                // check the payment requests
                const int privKeyId = i->second.walletSecretId;
                auto ws = m_walletSecrets.find(privKeyId);
                if (ws != m_walletSecrets.end() && ws->second.initialHeight == -1) {
                    ws->second.initialHeight = blockHeight;
                    needNewBloom = true; // make sure we let the remote know about our 'gap' addresses
                }

                for (auto pr : m_paymentRequests) {
                    if (pr->m_privKeyId == privKeyId) {
                        pr->addPayment(key, i->second.value, blockHeight);
                        wtx.userComment = pr->message();
                    }
                }
            }
            // and remember the transaction
            if (oldTx == m_txidCash.end()) {
                Q_ASSERT(walletTransactionId == m_nextWalletTransactionId);
                m_txidCash.insert(std::make_pair(wtx.txid, m_nextWalletTransactionId));
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
                auto iter = m_autoLockedOutputs.find(i->second);
                if (iter != m_autoLockedOutputs.end() && iter->second == ejectedTx)
                    m_autoLockedOutputs.erase(iter);
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
                for (auto pr : m_paymentRequests) {
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
        const QString txid = QString::fromStdString(tx.createHash().ToString());
        QString localdir = dir.arg(txid.left(2));
        boost::filesystem::create_directories(localdir.toStdString());
        QString filename = txid.mid(2);

        std::ofstream txSaver;
        txSaver.open((localdir + filename).toStdString());
        txSaver.write(tx.data().begin(), tx.size());
    } catch (const std::exception &e) {
        logFatal() << "Could not store transaction" << e.what();
        throw;
    }
}

Tx Wallet::loadTransaction(const uint256 &txid, Streaming::BufferPool &pool) const
{
    QString path = QString::fromStdString(txid.ToString());
    path.insert(2, '/');
    path = QString::fromStdString(m_basedir.string()) + "/" + path;

    QFile reader(path);
    if (reader.open(QIODevice::ReadOnly)) {
        pool.reserve(reader.size());
        reader.read(pool.begin(), reader.size());
        return Tx(pool.commit(reader.size()));
    }
    // return empty tx
    return Tx();
}

QList<PaymentRequest *> Wallet::paymentRequests() const
{
    QMutexLocker locker(&m_lock);
    return m_paymentRequests;
}

namespace {
QString renderAddress(const CKeyID &pubkeyhash)
{
    CashAddress::Content c;
    c.type = CashAddress::PUBKEY_TYPE;
    c.hash = std::vector<uint8_t>(pubkeyhash.begin(), pubkeyhash.end());
    auto s = CashAddress::encodeCashAddr(chainPrefix(), c);
    return QString::fromStdString(s).mid(chainPrefix().size() + 1);
}

QString renderAddress(const Streaming::ConstBuffer &outputScript)
{
    std::vector<std::vector<uint8_t> > vSolutions;
    Script::TxnOutType whichType;
    if (!Script::solver(outputScript, whichType, vSolutions))
        return QString();

    CKeyID keyID;
    switch (whichType)
    {
    case Script::TX_PUBKEY:
        keyID = CPubKey(vSolutions[0]).GetID();
        break;
    case Script::TX_PUBKEYHASH:
        keyID = CKeyID(uint160(vSolutions[0]));
        break;
    default:
        return QString();
    }

    return renderAddress(keyID);
}
}

void Wallet::fetchTransactionInfo(TransactionInfo *info, int txIndex)
{
    Q_ASSERT(info);
    QMutexLocker locker(&m_lock);
    auto iter = m_walletTransactions.find(txIndex);
    if (m_walletTransactions.end() == iter)
        throw std::runtime_error("Invalid tx-index");

    Tx tx = loadTransaction(iter->second.txid, FloweePay::pool(0));
    info->m_txSize = tx.size();

    // find out how many inputs and how many outputs there are.
    Tx::Iterator txIter(tx);
    // If we created this transaction (we have inputs in it anyway) then
    // also look up all the outputs from the file.
    const bool createdByUs = !iter->second.inputToWTX.empty();
    do {
        switch (txIter.next(Tx::PrevTxHash | Tx::OutputValue | Tx::OutputScript)) {
        case Tx::PrevTxHash: info->m_inputs.append(nullptr); break;
        case Tx::OutputValue: {
            TransactionOutputInfo *out = nullptr;
            if (createdByUs) {
                out = new TransactionOutputInfo(info);
                out->setForMe(false);
                out->setValue(txIter.longData());
            }
            info->m_outputs.append(out);
            break;
        }
        case Tx::OutputScript:
            assert(!info->m_outputs.isEmpty());
            if (createdByUs) {
                assert(info->m_outputs.back());
                info->m_outputs.back()->setAddress(renderAddress(txIter.byteData()));
            }
            break;
        default: break; // silence compiler warnings
        }
    } while (txIter.tag() != Tx::End);

    // probably only a couple of the inputs and outputs I have knowledge about,
    // since only those that use our addresses are stored in the wallet.
    // We find those and put the info objects in the assigned places.
    for (auto pair : iter->second.inputToWTX) {
        OutputRef ref(pair.second);
        auto w = m_walletTransactions.find(ref.txIndex());
        assert(w != m_walletTransactions.end());
        auto prevOut = w->second.outputs.find(ref.outputIndex());
        assert(prevOut != w->second.outputs.end());

        auto in = new TransactionInputInfo(info);
        in->setValue(prevOut->second.value);
        auto secret = m_walletSecrets.find(prevOut->second.walletSecretId);
        in->setAddress(renderAddress(secret->second.address));
        info->m_inputs[pair.first] = in;
    }
    // same for outputs
    for (auto o : iter->second.outputs) {
        auto secret = m_walletSecrets.find(o.second.walletSecretId);
        TransactionOutputInfo *out;
        if (createdByUs) { // reuse the one we created before from the raw Td.
            out = info->m_outputs[o.first];
            out->setForMe(true);
        }
        else {
            out = new TransactionOutputInfo(info);
            out->setValue(o.second.value);
            out->setAddress(renderAddress(secret->second.address));
        }
        out->setSpent(m_unspentOutputs.find(OutputRef(txIndex, o.first).encoded()) == m_unspentOutputs.end());
        info->m_outputs[o.first] = out;
    }
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
        keyID = CPubKey(vSolutions[0]).GetID();
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
    int numRegistredUnused = 0;
    for (auto &priv : m_walletSecrets) {
        bool use = priv.second.initialHeight > 0;
        if (!use && priv.second.reserved)
            use = true;
        if (!use) {
            // also listen to the first 10 unused addresses.
            use = ++numRegistredUnused <= 10;
        }

        if (use)
            m_segment->addKeyToFilter(priv.second.address, priv.second.initialHeight);
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
    m_singleAddressWallet = singleAddressWallet;
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

                    wtx->second.minedBlockHeight = WalletPriv::Rejected;
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

Tx::Output Wallet::txOutout(Wallet::OutputRef ref) const
{
    uint256 txid;
    {
        QMutexLocker locker(&m_lock);
        auto iter = m_walletTransactions.find(ref.txIndex());
        if (m_walletTransactions.end() == iter)
            throw std::runtime_error("Invalid ref");
        txid = iter->second.txid;
    }

    Tx tx = loadTransaction(txid, FloweePay::pool(0));
    if (tx.size() == 0)
        throw std::runtime_error("missing data");
    return tx.output(ref.outputIndex());
}

const CKey &Wallet::unlockKey(Wallet::OutputRef ref) const
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
    return iter3->second.privKey;
}

CKeyID Wallet::nextUnusedAddress()
{
    CKeyID answer;
    reserveUnusedAddress(answer);
    return answer;
}

int Wallet::reserveUnusedAddress(CKeyID &keyId)
{
    QMutexLocker locker(&m_lock);
    for (auto i = m_walletSecrets.begin(); i != m_walletSecrets.end(); ++i) {
        if (m_singleAddressWallet) {
            keyId = i->second.address;
            return i->first; // just return the first then.
        }
        if (i->second.initialHeight == -1 && !i->second.reserved) { // is unused address.
            i->second.reserved = true;
            keyId = i->second.address;
            rebuildBloom(); // make sure that we actually observe changes on this address
            return i->first;
        }
    }

    // no unused addresses, lets make some.
    int answer;
    for (int i = 0; i < 50; ++i) {
        WalletSecret secret;
        secret.privKey.MakeNewKey();

        const CPubKey pubkey = secret.privKey.GetPubKey();
        secret.address = pubkey.GetID();
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

namespace {
struct UnspentOutput {
    Wallet::OutputRef outputRef;
    qint64 value = 0;       // in satoshis
    int score = 0;          // the score gained by using this tx.
};

int scoreForSolution(size_t outputCount, int change, size_t unspentOutputCount)
{
    assert(unspentOutputCount > 0);
    assert(outputCount > 0);
    assert(change > 0);

    const int resultingOutputCount = unspentOutputCount - outputCount;
    int score = 0;
    // aim to keep our output count between 10 and 15
    if (resultingOutputCount > 10 && resultingOutputCount <= 15)
        score = 1000; // perfection
    else if (resultingOutputCount > 5 && resultingOutputCount < 15)
        score = 250;
    else if (resultingOutputCount < 25 && resultingOutputCount > 10)
        score = 250;
    else if (resultingOutputCount > 25)
        score -= (resultingOutputCount - 25) * 10;
    else
        score -= (5 - resultingOutputCount) * 10; // for the 0 - 5 range

    // in most cases no modifier is added due to change
    if (change < 100)
        score += 2000; // thats very nice (if over 0, that's for the miner)
    else if (change < 1000) // we would create very small UTXO, not nice.
        score -= 1000;
    else if (change < 5000) // ditto
        score -= 800;

    return score;
}
}

Wallet::OutputSet Wallet::findInputsFor(qint64 output, int feePerByte, int txSize, int64_t &change) const
{
    /*
     * The main selection criterea is the amount of outputs we have afterwards.
     *
     * The goal is to always have between 10 and 15 outputs of varying sizes in
     * our wallet, this makes sure we avoid being without confirmed outputs. Even
     * on medium-heavy usage.
     *
     *
     * As we assume the first items in the list of unspentOutputs are the oldest, all
     * we need to do is find the combination of inputs that works best.
     */

    const int currentBlockHeight = FloweePay::instance()->headerChainHeight();

    QList<UnspentOutput> unspentOutputs;
    std::map<uint64_t, size_t> utxosBySize;
    unspentOutputs.reserve(m_unspentOutputs.size());
    for (auto iter = m_unspentOutputs.begin(); iter != m_unspentOutputs.end(); ++iter) {
        if (m_autoLockedOutputs.find(iter->first) != m_autoLockedOutputs.end())
            continue;
        UnspentOutput out;
        out.value = iter->second;
        out.outputRef = OutputRef(iter->first);
        auto wtxIter = m_walletTransactions.find(out.outputRef.txIndex());
        Q_ASSERT(wtxIter != m_walletTransactions.end());

        int h = wtxIter->second.minedBlockHeight;
        if (h == WalletPriv::Unconfirmed) {
            out.score = -10; // unconfirmed.
        } else if (wtxIter->second.isCoinbase
                   && h + MATURATION_AGE >= m_lastBlockHeightSeen) {
            // don't spend an immature coinbase
            continue;
        } else {
            const int diff = currentBlockHeight - h;
            if (diff > 4024)
                out.score = 50;
            else if (diff > 1008)
                out.score = 30;
            else if (diff > 144)
                out.score = 10;
        }
        utxosBySize.insert(std::make_pair(iter->second, unspentOutputs.size()));
        unspentOutputs.push_back(out);
    }

    // First simply walk from oldest to newest until funded
    OutputSet bestSet;
    int bestScore = 0;
    bestSet.fee = txSize * feePerByte;
    for (auto iter = unspentOutputs.begin(); iter != unspentOutputs.end(); ++iter) {
        bestSet.outputs.push_back(OutputRef(iter->outputRef));
        bestSet.totalSats += iter->value;
        bestSet.fee += BYTES_PER_OUTPUT * feePerByte;
        bestScore += iter->score;

        if (output != -1 && bestSet.totalSats - bestSet.fee >= output)
            break;
    }
    if (output == -1) { // the magic number to return all outputs
        change = 0;
        return bestSet;
    }
    if (bestSet.totalSats - bestSet.fee < output)
        return OutputSet();

    bestScore += scoreForSolution(bestSet.outputs.size(),
                                     bestSet.totalSats - bestSet.fee - output, unspentOutputs.size());
    // try a new set.
    OutputSet current;
    int score = 0;
    current.fee = txSize * feePerByte;
    auto iterBySize = utxosBySize.end();
    while (iterBySize != utxosBySize.begin()) {
        --iterBySize;
        const auto &utxo = unspentOutputs.at(iterBySize->second);
        current.outputs.push_back(utxo.outputRef);
        current.totalSats += utxo.value;
        current.fee += BYTES_PER_OUTPUT * feePerByte;
        score += utxo.score;

        if (current.totalSats - current.fee >= output)
            break;
    }

    if (current.totalSats - current.fee >= output) {
        score += scoreForSolution(current.outputs.size(),
                                  current.totalSats - current.fee - output, unspentOutputs.size());

        // compare with the cost of oldest to newest.
        if (score > bestScore) {
            bestScore = score;
            bestSet = current;
        }
    }

    // Last we use random sets.
    for (int setIndex = 0; setIndex < 50; ++setIndex) {
        current = OutputSet();
        score = 0;
        current.fee = txSize * feePerByte;
        auto outputs = unspentOutputs;
        do {
            Q_ASSERT(!outputs.empty());
            const int index = static_cast<int>(rand() % outputs.size());
            Q_ASSERT(outputs.size() > index);
            const auto &out = outputs[index];
            current.outputs.push_back(out.outputRef);
            current.totalSats += out.value;
            current.fee += BYTES_PER_OUTPUT * feePerByte;
            score += out.score;

            outputs.removeAt(index); // take it.
        } while (current.totalSats - current.fee < output);

        score += scoreForSolution(current.outputs.size(),
                                  current.totalSats - current.fee - output, unspentOutputs.size());
        Q_ASSERT(current.totalSats - current.fee >= output);
        if (score > bestScore) {
            bestScore = score;
            bestSet = current;
        }
    }

    change = current.totalSats - current.fee - output;
    return current;
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
            auto tx = loadTransaction(iter->second.txid, FloweePay::pool(0));
            if (tx.data().size() > 64) {
                auto bc = std::make_shared<WalletInfoObject>(this, iter->first, tx);
                bc->moveToThread(thread());
                logDebug() << "  broadcasting transaction" << tx.createHash() << tx.size();
                m_broadcastingTransactions.append(bc);
                FloweePay::instance()->p2pNet()->connectionManager().broadcastTransaction(bc);
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

void Wallet::createNewPrivateKey(int currentBlockheight)
{
    QMutexLocker locker(&m_lock);
    WalletSecret secret;
    secret.privKey.MakeNewKey();

    const CPubKey pubkey = secret.privKey.GetPubKey();
    secret.address = pubkey.GetID();
    secret.initialHeight = currentBlockheight;
    m_walletSecrets.insert(std::make_pair(m_nextWalletSecretId++, secret));
    m_secretsChanged = true;
    saveSecrets();

    m_segment->addKeyToFilter(pubkey.GetID(), currentBlockheight);

    rebuildBloom();
}

bool Wallet::addPrivateKey(const QString &privKey, int startBlockHeight)
{
    QMutexLocker locker(&m_lock);
    CBase58Data encodedData;
    auto bytes = privKey.toLatin1();
    encodedData.SetString(bytes.constData());
    if (encodedData.isMainnetPrivKey() || encodedData.isTestnetPrivKey()) {
        WalletSecret secret;
        secret.privKey.Set(encodedData.data().begin(), encodedData.data().begin() + 32,
                encodedData.data().size() > 32 && encodedData.data()[32] == 1);

        // TODO loop over secrets and avoid adding one privkey twice.

        const CPubKey pubkey = secret.privKey.GetPubKey();
        secret.address = pubkey.GetID();
        secret.initialHeight = startBlockHeight;
        m_walletSecrets.insert(std::make_pair(m_nextWalletSecretId++, secret));
        m_secretsChanged = true;
        saveSecrets();

        m_segment->addKeyToFilter(pubkey.GetID(), startBlockHeight);

        rebuildBloom();
        return true;
    }
    logFatal() << "ERROR. Wallet: added string is not a private key";
    return false;
}

void Wallet::loadSecrets()
{
    std::ifstream in((m_basedir / "secrets.dat").string());
    if (!in.is_open())
        throw std::runtime_error("Missing secrets.dat");
    QMutexLocker locker(&m_lock);
    const auto dataSize = boost::filesystem::file_size(m_basedir / "secrets.dat");
    Streaming::BufferPool pool(dataSize);
    in.read(pool.begin(), dataSize);
    Streaming::MessageParser parser(pool.commit(dataSize));
    WalletSecret secret;
    int index = 0;
    while (parser.next() == Streaming::FoundTag) {
        if (parser.tag() == WalletPriv::Separator) {
            if (index > 0 && secret.address.size() > 0) {
                m_walletSecrets.insert(std::make_pair(index, secret));
                m_nextWalletSecretId = std::max(m_nextWalletSecretId, index);
            }
            secret = WalletSecret();
        }
        else if (parser.tag() == WalletPriv::Index) {
            index = parser.intData();
        }
        else if (parser.tag() == WalletPriv::PrivKey) {
            auto d = parser.unsignedBytesData();
            secret.privKey.Set(d.begin(), d.end(), true);
        }
        else if (parser.tag() == WalletPriv::PubKeyHash) {
            auto d = parser.bytesDataBuffer();
            secret.address = CKeyID(d.begin());
        }
        else if (parser.tag() == WalletPriv::HeightCreated) {
            secret.initialHeight = parser.intData();
        }
        else if (parser.tag() == WalletPriv::IsSingleAddressWallet) {
            m_singleAddressWallet = parser.boolData();
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
    Streaming::BufferPool pool(m_walletSecrets.size() * 70);
    Streaming::MessageBuilder builder(pool);
    for (const auto &item : m_walletSecrets) {
        builder.add(WalletPriv::Index, item.first);
        builder.addByteArray(WalletPriv::PrivKey, item.second.privKey.begin(), item.second.privKey.size());
        builder.addByteArray(WalletPriv::PubKeyHash, item.second.address.begin(), item.second.address.size());
        if (item.second.initialHeight > 0)
            builder.add(WalletPriv::HeightCreated, item.second.initialHeight);

        builder.add(WalletPriv::Separator, true);
    }
    if (m_singleAddressWallet)
        builder.add(WalletPriv::IsSingleAddressWallet, true);
    auto data = builder.buffer();

    try {
        boost::filesystem::create_directories(m_basedir);
        boost::filesystem::remove(m_basedir / "secrets.dat~");

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

void Wallet::loadWallet()
{
    std::ifstream in((m_basedir / "wallet.dat").string());
    if (!in.is_open())
        return;
    QMutexLocker locker(&m_lock);
    const auto dataSize = boost::filesystem::file_size(m_basedir / "wallet.dat");
    Streaming::BufferPool pool(dataSize);
    in.read(pool.begin(), dataSize);
    Streaming::MessageParser parser(pool.commit(dataSize));
    WalletTransaction wtx;
    int index = 0;
    int inputIndex = -1;
    int outputIndex = -1;
    int tmp = 0;
    WalletPriv::InputLockStateEnum inputLock = WalletPriv::Unlocked;
    int autoLockId = -1;
    Output output;
    QSet<int> newTx;
    int highestBlockHeight = 0;
    PaymentRequest *pr = nullptr;
    OutputRef paymentRequestRef;
    while (parser.next() == Streaming::FoundTag) {
        if (parser.tag() == WalletPriv::Separator) {
            assert(index > 0);
            assert(m_walletTransactions.find(index) == m_walletTransactions.end());
            assert(!wtx.inputToWTX.empty() || !wtx.outputs.empty());
            m_walletTransactions.insert(std::make_pair(index, wtx));
            m_txidCash.insert(std::make_pair(wtx.txid, index));
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
        else if (parser.tag() == WalletPriv::InputIndex) {
            inputIndex = parser.intData();
        }
        else if (parser.tag() == WalletPriv::InputLockState) {
            if (parser.intData() == WalletPriv::AutoLocked || parser.intData() == WalletPriv::UserLocked)
                inputLock = static_cast<WalletPriv::InputLockStateEnum>(parser.intData());
        }
        else if (parser.tag() == WalletPriv::InputLockAutoSpender) {
            autoLockId = parser.intData();
        }
        else if (parser.tag() == WalletPriv::InputSpendsTx) {
            tmp = parser.intData();
        }
        else if (parser.tag() == WalletPriv::InputSpendsOutput) {
            assert(inputIndex >= 0);
            assert(tmp != 0);
            assert(parser.longData() < 0xFFFF);
            OutputRef ref(tmp, parser.intData());
            wtx.inputToWTX.insert(std::make_pair(inputIndex, ref.encoded()));
            if (inputLock == WalletPriv::AutoLocked && autoLockId != -1)
                m_autoLockedOutputs.insert(std::make_pair(ref.encoded(), autoLockId));
            tmp = 0;
            inputLock = WalletPriv::Unlocked;
        }
        else if (parser.tag() == WalletPriv::OutputIndex) {
            outputIndex = parser.intData();
        }
        else if (parser.tag() == WalletPriv::OutputValue) {
            output.value = parser.longData();
        }
        else if (parser.tag() == WalletPriv::KeyStoreIndex) {
            assert(outputIndex >= 0);
            assert(output.value > 0);
            output.walletSecretId = parser.intData();
            wtx.outputs.insert(std::make_pair(outputIndex, output));
            outputIndex = -1;
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
            pr->m_incomingOutputRefs.append(paymentRequestRef.encoded());
        }
        else if (parser.tag() == WalletPriv::PaymentRequestPaid) {
            pr->m_amountSeen = parser.longData();
            if (pr->m_amountSeen >= pr->m_amountRequested)
                pr->m_paymentState = PaymentRequest::PaymentSeenOk;
        }
    }

    // after inserting all outputs during load, now remove all inputs these tx's spent.
    for (auto index : newTx) {
        auto iter = m_walletTransactions.find(index);
        assert(iter != m_walletTransactions.end());

        if (iter->second.minedBlockHeight != WalletPriv::Unconfirmed) {
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
#endif

    m_walletChanged = false;
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
    Streaming::BufferPool pool(m_walletTransactions.size() * 110 + m_name.size() * 3 + 100);
    Streaming::MessageBuilder builder(pool);
    for (const auto &item : m_walletTransactions) {
        builder.add(WalletPriv::Index, item.first);
        builder.add(WalletPriv::TxId, item.second.txid);
        builder.add(WalletPriv::BlockHash, item.second.minedBlock);
        builder.add(WalletPriv::BlockHeight, item.second.minedBlockHeight);
        if (item.second.isCoinbase)
            builder.add(WalletPriv::OutputFromCoinbase, true);
        for (auto i = item.second.inputToWTX.begin(); i != item.second.inputToWTX.end(); ++i) {
            builder.add(WalletPriv::InputIndex, i->first);
            const OutputRef ref(i->second);
            builder.add(WalletPriv::InputSpendsTx, ref.txIndex());
            builder.add(WalletPriv::InputSpendsOutput, ref.outputIndex());
        }
        for (auto i = item.second.outputs.begin(); i != item.second.outputs.end(); ++i) {
            builder.add(WalletPriv::OutputIndex, i->first);
            builder.add(WalletPriv::OutputValue, i->second.value);
            builder.add(WalletPriv::KeyStoreIndex, i->second.walletSecretId);

            auto lock = m_autoLockedOutputs.find(OutputRef(item.first, i->first).encoded());
            if (lock != m_autoLockedOutputs.end()) {
                builder.add(WalletPriv::InputLockState, WalletPriv::AutoLocked);
                builder.add(WalletPriv::InputLockAutoSpender, lock->second);
            }
        }
        if (!item.second.userComment.isEmpty())
            builder.add(WalletPriv::UserComment, item.second.userComment.toStdString());
        builder.add(WalletPriv::Separator, true);
    }
    builder.add(WalletPriv::LastSynchedBlock, m_segment->lastBlockSynched());
    builder.add(WalletPriv::WalletName, m_name.toUtf8().toStdString());

    for (auto pr : m_paymentRequests) {
        builder.add(WalletPriv::PaymentRequestType, 0); // bip21 is the only one supported right now
        builder.add(WalletPriv::PaymentRequestAddress, pr->m_privKeyId);
        if (!pr->m_message.isEmpty())
            builder.add(WalletPriv::PaymentRequestMessage, pr->m_message.toUtf8().constData());
        assert(pr->m_amountRequested >= 0); // never negative
        if (pr->m_amountRequested > 0)
            builder.add(WalletPriv::PaymentRequestAmount, (uint64_t) pr->m_amountRequested);
        if (pr->m_useLegacyAddressFormat)
            builder.add(WalletPriv::PaymentRequestOldAddress, true);

        for (auto outRefNum : pr->m_incomingOutputRefs) {
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
        else if (m_autoLockedOutputs.find(utxo.first) != m_autoLockedOutputs.end())
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
