/*
 * This file is part of the Flowee project
 * Copyright (C) 2020 Tom Zander <tomz@freedommail.ch>
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

#include <primitives/FastTransaction.h>
#include <primitives/script.h>
#include <streaming/BufferPool.h>
#include <streaming/MessageBuilder.h>
#include <streaming/MessageParser.h>
#include <base58.h>
#include <cashaddr.h>

#include <FloweePay.h>
#include <QFile>
#include <QSet>
#include <QTimer>

// #define DEBUGUTXO

/*
 * Fee estimation needs to know how much space is used per input.
 * 32 + 4 for the prevTx
 * Then 72 + 33 bytes for the signature and pub-key (+-1).
 * Using schnorr we can gain 8 bytes for the signature.
 */
constexpr int BYTES_PER_OUTPUT = 149;

/*
 * TODO
 *
 * - the wallet should allow a simple encryption of the private keys based on a passphrase
 *   The loading would then skip over the private keys, we need a separate loading for when we need signing.
 *
 * - Saving should be done more log-file style so we are not too dependent on keeping things in memory.
 *   The fileformat means we can just save dozens of files each saving a series of transactions.
 *   Please note that order is still relevant for the UTXO!
 *
 * - each wallet should have some booleans
 *    - allow lookup through indexing-servers (i.e. introduce privacy-leak)
 *    - auto-create new addresses (i.e. user doesn't expect us to be restricted to one priv-key)
 *    - use HD wallet.  (i.e. we generate new addresses from one priv key only)
 *    - sign using schnorr
 */

namespace {

enum SaveTags {
    Separator = 0,
    Index,
    PrivKey,
    PrivKeyEncrypted,
    PubKeyHash,
    HeightCreated, // for an address / privkey this is used to check which blocks to query

    IsSingleAddressWallet, // bool

    TxId = 15,
    BlockHash,
    BlockHeight,

    InputIndex,
    InputSpendsTx, // int that refers to index of the WTX it spends
    InputSpendsOutput, // input that refers to the output in the WTX it spends

    OutputIndex,
    OutputValue, // in Satoshi
    KeyStoreIndex, // int that refers to the index of the privkey.

    LastSynchedBlock,
    WalletName,

    /// Used after InputIndex to mark the lock-state of an input.
    InputLockState, // positive number, see InputLockStateEnum
    InputLockAutoSpender // the tx that locked an output (index in m_walletTransactions
};

enum InputLockStateEnum {
    Unlocked = 0, // default value when not saved
    AutoLocked,   // Locked due to it being spent. Locked until mined.
    UserLocked    // User locked this output
};

enum SpecialBlockHeight {
    Genesis = 0,
    Unconfirmed = -1,   // a tx marked with this height is waiting to get confirmed
    Rejected = -2       // a tx marked with this height can no longer be mined
};

// helper method for sortTransactions
void copyTx(size_t index, std::deque<Tx> &answer, const std::vector<QSet<int> > &order, std::vector<bool> &done, const std::deque<Tx> &in) {
    if (done.at(index))
        return;
    done[index] = true;
    for (auto dependencies : order.at(index)) {
        copyTx(dependencies, answer, order, done, in);
    }
    answer.push_back(in.at(index));
}

}


// /////////////////////////////////////////////////

std::deque<Tx> WalletPriv::sortTransactions(const std::deque<Tx> &in) {
    if (in.size() < 2)
        return in;
    boost::unordered_map<uint256, int, HashShortener> txHashes;
    for (size_t i = 0; i < in.size(); ++i) {
        txHashes.insert(std::make_pair(in.at(i).createHash(), int(i)));
    }
    std::vector<QSet<int> > order;
    order.reserve(in.size());
    for (size_t i = 0; i < in.size(); ++i) {
        Tx::Iterator iter(in.at(i));
        QSet<int> depends;
        for (auto input : Tx::findInputs(iter)) {
            auto prev = txHashes.find(input.txid);
            if (prev != txHashes.end()) {
                depends.insert(prev->second);
            }
        }
        order.push_back(depends);
    }
    std::vector<bool> done(in.size(), false);
    std::deque<Tx> answer;
    for (size_t i = 0; i < in.size(); ++i) {
        copyTx(i, answer, order, done, in);
    }
    return answer;
}


// ////////////////////////////////////////////////////

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

Wallet::WalletTransaction Wallet::createWalletTransactionFromTx(const Tx &tx, const uint256 &txid)
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
            ++inputIndex;
            auto i = m_txidCash.find(iter.uint256Data());
            prevTx.setTxIndex((i != m_txidCash.end()) ? i->second : 0);
            if (i != m_txidCash.end())
                logDebug() << "  Input:" << inputIndex << "prevTx:" << iter.uint256Data() << Log::Hex << i->second << prevTx.encoded();
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
        if (wtx.outputs.empty() && wtx.inputToWTX.empty()) {
            // no connection to our UTXOs
            if (--m_bloomScore < 25)
                rebuildBloom();
            return;
        }
        wtx.minedBlockHeight = Unconfirmed;

        // Mark UTXOs locked that this tx spent to avoid double spending them.
        for (auto i = wtx.inputToWTX.begin(); i != wtx.inputToWTX.end(); ++i) {
            m_autoLockedOutputs.insert(std::make_pair(i->second, m_nextWalletTransactionId));
        }

        // insert new UTXOs
        for (auto i = wtx.outputs.begin(); i != wtx.outputs.end(); ++i) {
            uint64_t key = m_nextWalletTransactionId;
            key <<= 16;
            key += i->first;
            logDebug() << "   inserting output"<< i->first << Log::Hex << i->second.walletSecretId << key;
            m_unspentOutputs.insert(std::make_pair(key, i->second.value));
        }

        // and remember the transaction
        m_txidCash.insert(std::make_pair(wtx.txid, m_nextWalletTransactionId));
        m_walletTransactions.insert(std::make_pair(m_nextWalletTransactionId++, wtx));
        m_walletChanged = true;

        logCritical() << "Wallet" << m_segment->segmentId() << "claims" << tx.createHash() << "[unconfirmed]";
    } // mutex scope
    saveTransaction(tx);

    emit utxosChanged();
    emit appendedTransactions(firstNewTransaction, 1);
}

void Wallet::newTransactions(const BlockHeader &header, int blockHeight, const std::deque<Tx> &blockTransactions)
{
    auto transactions = WalletPriv::sortTransactions(blockTransactions);
    std::deque<Tx> transactionsToSave;
    std::set<int> ejectedTransactions;
    int firstNewTransaction;
    {
        QMutexLocker locker(&m_lock);
        firstNewTransaction = m_nextWalletTransactionId;
        for (auto tx: transactions) {
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
            const bool wasUnconfirmed = wtx.minedBlockHeight == Unconfirmed;
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

            if (!wasUnconfirmed) { // unconfirmed transactions already had their outputs added
                // insert new UTXOs
                for (auto i = wtx.outputs.begin(); i != wtx.outputs.end(); ++i) {
                    uint64_t key = m_nextWalletTransactionId;
                    key <<= 16;
                    key += i->first;
                    logDebug() << "   inserting output"<< i->first << Log::Hex << i->second.walletSecretId << key;
                    m_unspentOutputs.insert(std::make_pair(key, i->second.value));
                }
            }
            // and remember the transaction
            if (oldTx == m_txidCash.end()) {
                Q_ASSERT(walletTransactionId == m_nextWalletTransactionId);
                m_txidCash.insert(std::make_pair(wtx.txid, m_nextWalletTransactionId));
                m_walletTransactions.insert(std::make_pair(m_nextWalletTransactionId++, wtx));
                transactionsToSave.push_back(tx);
            }
            m_walletChanged = true;

            logCritical() << "Wallet" << m_segment->segmentId() << "claims" << tx.createHash() << "@" << blockHeight;
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
            wtx.minedBlockHeight = Rejected;
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
            }
        }

    } // mutex scope
    if (!transactionsToSave.empty()) {
        emit utxosChanged();
        emit appendedTransactions(firstNewTransaction, transactionsToSave.size());
    }

    for (auto tx : transactionsToSave) { // save the Tx to disk.
        saveTransaction(tx);
    }
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

Tx Wallet::loadTransaction(const uint256 &txid, Streaming::BufferPool *pool)
{
    Q_ASSERT(pool);
    QString path = QString::fromStdString(txid.ToString());
    path.insert(2, '/');
    path = QString::fromStdString(m_basedir.string()) + "/" + path;

    QFile reader(path);
    if (reader.open(QIODevice::ReadOnly)) {
        pool->reserve(reader.size());
        reader.read(pool->begin(), reader.size());
        return Tx(pool->commit(reader.size()));
    }
    // return empty tx
    return Tx();
}


int Wallet::findSecretFor(const Streaming::ConstBuffer &outputScript)
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
    for (auto priv : m_walletSecrets) {
        if (priv.second.initialHeight > 0)
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
    for (int i = 0; i < m_broadcastingTransactions.size(); ++i) {
        if (m_broadcastingTransactions.at(i)->txIndex() == txIndex) {
            m_broadcastingTransactions.removeAt(i);
            // TODO process the 'success' bool.
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

const uint256 &Wallet::txid(Wallet::OutputRef ref) const
{
    QMutexLocker locker(&m_lock);
    auto iter = m_walletTransactions.find(ref.txIndex());
    if (m_walletTransactions.end() == iter)
        throw std::runtime_error("Invalid ref");
    return iter->second.txid;
}

Tx::Output Wallet::txOutout(Wallet::OutputRef ref) const
{
    QString txid;
    {
        QMutexLocker locker(&m_lock);
        auto iter = m_walletTransactions.find(ref.txIndex());
        if (m_walletTransactions.end() == iter)
            throw std::runtime_error("Invalid ref");
        txid = QString::fromStdString(iter->second.txid.ToString());
    }

    QString filename("%1/%2/%3");
    filename = filename.arg(QString::fromStdString(m_basedir.string()));
    filename = filename.arg(txid.left(2));
    filename = filename.arg(txid.mid(2));

    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly))
        throw std::runtime_error("missing data");

    const int fileSize = file.size();
    Streaming::BufferPool pool(fileSize);
    file.read(pool.begin(), fileSize);
    Tx tx(pool.commit(fileSize));
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

CKeyID Wallet::nextChangeAddress()
{
    QMutexLocker locker(&m_lock);
    for (auto i = m_walletSecrets.begin(); i != m_walletSecrets.end(); ++i) {
        if (m_singleAddressWallet)
            return i->second.address; // just return the first then.
        if (i->second.initialHeight == -1) // is change address.
            return i->second.address;
    }

    // no change addresses, lets make some.
    CKeyID answer;
    for (int i = 0; i < 50; ++i) {
        WalletSecret secret;
        secret.privKey.MakeNewKey();

        const CPubKey pubkey = secret.privKey.GetPubKey();
        secret.address = pubkey.GetID();
        if (i == 0)
            answer = secret.address;
        m_walletSecrets.insert(std::make_pair(m_nextWalletSecretId++, secret));
    }
    m_secretsChanged = true;
    saveSecrets();

    return answer;
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
        if (h == Unconfirmed) {
            out.score = -10; // unconfirmed.
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
    emit lastBlockSynchedChanged();

    if (height == FloweePay::instance()->headerChainHeight()) {
logCritical() << "Check if there are any transactions to rebroadcast";
        // we are (again) up-to-date.
        // Lets broadcast any transactions that have not yet been confirmed.
        m_broadcastingTransactions.clear();
        Streaming::BufferPool *pool = nullptr;

        for (auto iter = m_walletTransactions.begin();
             iter != m_walletTransactions.end(); ++iter) {

            if (iter->second.minedBlockHeight == Unconfirmed) {
                if (pool == nullptr) {
                    logCritical() << "Starting to rebroadcast transactions";
                    pool = new Streaming::BufferPool();
                }

                auto tx = loadTransaction(iter->second.txid, pool);
                if (tx.data().size() > 64) {
                    auto bc = std::make_shared<WalletInfoObject>(this, iter->first, tx);
                    m_broadcastingTransactions.append(bc);
                    FloweePay::instance()->p2pNet()->connectionManager().broadcastTransaction(bc);
                }
                else {
                    logCritical() << "Unconfirmed transaction could not be found on disk!";
                }
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
        if (parser.tag() == Separator) {
            if (index > 0 && secret.address.size() > 0) {
                m_walletSecrets.insert(std::make_pair(index, secret));
                m_nextWalletSecretId = std::max(m_nextWalletSecretId, index);
            }
            secret = WalletSecret();
        }
        else if (parser.tag() == Index) {
            index = parser.intData();
        }
        else if (parser.tag() == PrivKey) {
            auto d = parser.unsignedBytesData();
            secret.privKey.Set(d.begin(), d.end(), true);
        }
        else if (parser.tag() == PubKeyHash) {
            auto d = parser.bytesDataBuffer();
            secret.address = CKeyID(d.begin());
        }
        else if (parser.tag() == HeightCreated) {
            secret.initialHeight = parser.intData();
        }
        else if (parser.tag() == IsSingleAddressWallet) {
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
        builder.add(Index, item.first);
        builder.addByteArray(PrivKey, item.second.privKey.begin(), item.second.privKey.size());
        builder.addByteArray(PubKeyHash, item.second.address.begin(), item.second.address.size());
        if (item.second.initialHeight > 0)
            builder.add(HeightCreated, item.second.initialHeight);

        builder.add(Separator, true);
    }
    if (m_singleAddressWallet)
        builder.add(IsSingleAddressWallet, true);
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
    InputLockStateEnum inputLock = Unlocked;
    int autoLockId = -1;
    Output output;
    QSet<int> newTx;
    while (parser.next() == Streaming::FoundTag) {
        if (parser.tag() == Separator) {
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
        }
        else if (parser.tag() == Index) {
            index = parser.intData();
        }
        else if (parser.tag() == TxId) {
            wtx.txid = parser.uint256Data();
        }
        else if (parser.tag() == BlockHash) {
            wtx.minedBlock = parser.uint256Data();
        }
        else if (parser.tag() == BlockHeight) {
            wtx.minedBlockHeight = parser.intData();
            m_segment->blockSynched(wtx.minedBlockHeight);
        }
        else if (parser.tag() == InputIndex) {
            inputIndex = parser.intData();
        }
        else if (parser.tag() == InputLockState) {
            if (parser.intData() == AutoLocked || parser.intData() == UserLocked)
                inputLock = static_cast<InputLockStateEnum>(parser.intData());
        }
        else if (parser.tag() == InputLockAutoSpender) {
            autoLockId = parser.intData();
        }
        else if (parser.tag() == InputSpendsTx) {
            tmp = parser.intData();
        }
        else if (parser.tag() == InputSpendsOutput) {
            assert(inputIndex >= 0);
            assert(tmp != 0);
            assert(parser.longData() < 0xFFFF);
            OutputRef ref(tmp, parser.intData());
            wtx.inputToWTX.insert(std::make_pair(inputIndex, ref.encoded()));
            if (inputLock == AutoLocked && autoLockId != -1)
                m_autoLockedOutputs.insert(std::make_pair(ref.encoded(), autoLockId));
            tmp = 0;
            inputLock = Unlocked;
        }
        else if (parser.tag() == OutputIndex) {
            outputIndex = parser.intData();
        }
        else if (parser.tag() == OutputValue) {
            output.value = parser.longData();
        }
        else if (parser.tag() == KeyStoreIndex) {
            assert(outputIndex >= 0);
            assert(output.value > 0);
            output.walletSecretId = parser.intData();
            wtx.outputs.insert(std::make_pair(outputIndex, output));
            outputIndex = -1;
        }
        else if (parser.tag() == WalletName) {
            assert(parser.isString());
            m_name = QString::fromUtf8(parser.stringData().c_str(), parser.dataLength());
        }
        else if (parser.tag() == LastSynchedBlock) {
            logFatal() << "last Block Sync" << parser.intData();
            m_segment->blockSynched(parser.intData());
            m_segment->blockSynched(parser.intData()); // yes, twice.
        }
    }

    // after inserting all outputs during load, now remove all inputs these tx's spent.
    for (auto index : newTx) {
        auto iter = m_walletTransactions.find(index);
        assert(iter != m_walletTransactions.end());

        if (iter->second.minedBlockHeight == Unconfirmed) {
            // this indicates it has not been mined
            // Instead of removing the utxos, we lock them.









        }
        // remove UTXOs this Tx spent
        for (auto i = iter->second.inputToWTX.begin(); i != iter->second.inputToWTX.end(); ++i) {
            auto utxo = m_unspentOutputs.find(i->second);
            assert(utxo != m_unspentOutputs.end()); // Loading should be done in-order to avoid this.
            if (utxo != m_unspentOutputs.end())
                m_unspentOutputs.erase(utxo);
        }
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
    for (auto tx : m_walletTransactions) {
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
    rebuildBloom();
    emit utxosChanged();
}

void Wallet::saveWallet()
{
    QMutexLocker locker(&m_lock);
    if (!m_walletChanged)
        return;
    Streaming::BufferPool pool(m_walletTransactions.size() * 110 + m_name.size() * 3 + 100);
    Streaming::MessageBuilder builder(pool);
    for (const auto &item : m_walletTransactions) {
        builder.add(Index, item.first);
        builder.add(TxId, item.second.txid);
        builder.add(BlockHash, item.second.minedBlock);
        builder.add(BlockHeight, item.second.minedBlockHeight);
        for (auto i = item.second.inputToWTX.begin(); i != item.second.inputToWTX.end(); ++i) {
            builder.add(InputIndex, i->first);
            uint64_t key = i->second;
            int outIndex = static_cast<int>(key & 0xFF);
            key = key >> 16;
            auto lock = m_autoLockedOutputs.find(key);
            if (lock != m_autoLockedOutputs.end()) {
                builder.add(InputLockState, AutoLocked);
                builder.add(InputLockAutoSpender, lock->second);
            }

            builder.add(InputSpendsTx, key);
            builder.add(InputSpendsOutput, outIndex);
        }
        for (auto i = item.second.outputs.begin(); i != item.second.outputs.end(); ++i) {
            builder.add(OutputIndex, i->first);
            builder.add(OutputValue, i->second.value);
            builder.add(KeyStoreIndex, i->second.walletSecretId);
        }
        builder.add(Separator, true);
    }
    builder.add(LastSynchedBlock, m_segment->lastBlockSynched());
    builder.add(WalletName, m_name.toUtf8().toStdString());

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

qint64 Wallet::balance() const
{
    QMutexLocker locker(&m_lock);
    uint64_t amount = 0;
    for (auto utxo : m_unspentOutputs) {
        if (m_autoLockedOutputs.find(utxo.first) == m_autoLockedOutputs.end())
            amount += utxo.second;
    }
    return amount;
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
    for (auto wtx : m_walletTransactions) {
        count += wtx.second.outputs.size();
    }
    return count;
}

Wallet::OutputRef::OutputRef(uint64_t encoded)
{
    m_outputIndex = encoded & 0xFFFF;
    encoded >>= 16;
    assert(encoded < 0xEFFFFFFF);
    m_txid = encoded;
    assert(m_txid >= 0);
}

Wallet::OutputRef::OutputRef(int txIndex, int outputIndex)
    : m_txid(txIndex),
      m_outputIndex(outputIndex)
{
    assert(txIndex >= 0); // zero is the 'invalid' state, which is valid here...
    assert(outputIndex >= 0);
    assert(outputIndex < 0XFFFF);
}

uint64_t Wallet::OutputRef::encoded() const
{
    uint64_t answer = m_txid;
    answer <<= 16;
    answer += m_outputIndex;
    return answer;
}


// //////////////////////////////////////////////////

WalletInfoObject::WalletInfoObject(Wallet *wallet, int txIndex, const Tx &tx)
    : BroadcastTxData(tx),
      m_wallet(wallet),
      m_txIndex(txIndex)
{
    connect(this, SIGNAL(finished(int,bool)),
            m_wallet, SLOT(broadcastTxFinished(int,bool)), Qt::QueuedConnection);
}

void WalletInfoObject::txRejected(RejectReason reason, const std::string &message)
{
    // reason is hinted using BroadcastTxData::RejectReason
    logCritical() << "Transaction rejected" << reason << message;
    ++m_rejectedPeerCount;
}

void WalletInfoObject::sentOne()
{
    if (++m_sentPeerCount >= 2) {
        // if two peers requested the tx, then wait a bit and check on the status
        QTimer::singleShot(5 * 1000, this, SLOT(checkState()));
    }
}

void WalletInfoObject::checkState()
{
    if (m_sentPeerCount > 2) {
        emit finished(m_txIndex, m_sentPeerCount - m_rejectedPeerCount >= 1);
    }
}

int WalletInfoObject::txIndex() const
{
    return m_txIndex;
}

uint16_t WalletInfoObject::privSegment() const
{
    assert(m_wallet);
    assert(m_wallet->segment());
    return m_wallet->segment()->segmentId();
}
