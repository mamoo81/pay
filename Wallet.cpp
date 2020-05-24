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

#include <primitives/FastTransaction.h>
#include <primitives/script.h>
#include <streaming/BufferPool.h>
#include <streaming/MessageBuilder.h>
#include <streaming/MessageParser.h>
#include <base58.h>
#include <cashaddr.h>

#include <QSet>

/*
 * TODO
 *
 * - Make the mempool based addTransaction also do something.
 * - the wallet should allow a simple encryption of the private keys based on a passphrase
 *   The loading would then skip over the private keys, we need a separate loading for when we need signing.
 *
 * - Saving should be done more log-file style so we are not too dependent on keeping things in memory.
 *   The fileformat means we can just save dozens of files each saving a series of transactions.
 *   Please note that order is still relevant for the UTXO!
 */

namespace {

enum SaveTags {
    Separator = 0,
    Index,
    PrivKey,
    PrivKeyEncrypted,
    PubKeyHash,
    HeightCreated, // for an address / privkey this is used to check which blocks to query

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
    WalletName
};

// helper method
void copyTx(size_t index, std::deque<Tx> &answer, const std::vector<QSet<int> > &order, std::vector<bool> &done, const std::deque<Tx> &in) {
    if (done.at(index))
        return;
    done[index] = true;
    for (auto dependencies : order.at(index)) {
        copyTx(dependencies, answer, order, done, in);
    }
    answer.push_back(in.at(index));
}

// we may have transactions that spend outputs created within the same block.
// this method will make sure that we sort them so those that spend others
// are sorted after the ones they spent.
std::deque<Tx> sortTransactions(const std::deque<Tx> &in) {
    if (in.size() < 2)
        return in;
    boost::unordered_map<uint256, int, HashShortener> txHashes;
    for (size_t i = 0; i < in.size(); ++i) {
        txHashes.insert(std::make_pair(in.at(i).createHash(), int(i)));
    }
    std::vector<QSet<int> > order;
    order.resize(in.size());
    for (size_t i = 0; i < in.size(); ++i) {
        Tx::Iterator iter(in.at(i));
        QSet<int> depends;
        for (auto input : Tx::findInputs(iter)) {
            auto prev = txHashes.find(input.txid);
            if (prev != txHashes.end()) {
                depends.insert(prev->second);
            }
        }
    }
    std::vector<bool> done(in.size(), false);
    std::deque<Tx> answer;
    for (size_t i = 0; i < in.size(); ++i) {
        copyTx(i, answer, order, done, in);
    }
    return answer;
}

}


Wallet::Wallet(const boost::filesystem::path &basedir, uint16_t segmentId)
    : m_segment(new PrivacySegment(segmentId, this)),
      m_basedir(basedir / QString("wallet-%1/").arg(segmentId).toStdString())
{
    loadSecrets();
    loadWallet();
    rebuildBloom();
    if (m_name.isEmpty())
        m_name = QString("unnamed-%1").arg(segmentId);
}


void Wallet::newTransactions(const BlockHeader &header, int blockHeight, const std::deque<Tx> &blockTransactions)
{
    auto transactions = sortTransactions(blockTransactions);
    std::deque<Tx> transactionsToSave;
    int firstNewTransaction;
    {
        QMutexLocker locker(&m_lock);
        firstNewTransaction = m_nextWalletTransactionId;
        for (auto tx: transactions) {
            WalletTransaction wtx;
            wtx.txid = tx.createHash();
            wtx.minedBlock = header.createHash();
            wtx.minedBlockHeight = blockHeight;

            Tx::Iterator iter(tx);
            uint256 prevTxHash;
            if (m_txidCash.find(wtx.txid) != m_txidCash.end()) // already known
                continue;

            uint64_t prevTx = 0;
            int inputIndex = -1;
            int outputIndex = -1;
            Output output;
            logDebug() << "new tx." << wtx.txid;
            while (iter.next() != Tx::End) {
                if (iter.tag() == Tx::PrevTxHash) {
                    ++inputIndex;
                    auto i = m_txidCash.find(iter.uint256Data());
                    prevTx = (i != m_txidCash.end()) ? (i->second << 16) : 0;
                    if (i != m_txidCash.end())
                        logDebug() << "  Input:" << inputIndex << "prevTx:" << iter.uint256Data() << Log::Hex << i->second << prevTx;
                } else  if (iter.tag() == Tx::PrevTxIndex) {
                    if (prevTx > 0) { // we know the prevTx
                        assert(iter.longData() < 0xFFFF); // it would break our scheme
                        prevTx += iter.longData();

                        auto utxo = m_unspentOutputs.find(prevTx);
                        if (utxo != m_unspentOutputs.end()) {
                            // input is spending one of our UTXOs
                            logDebug() << "   -> spent UTXO";
                            wtx.inputToWTX.insert(std::make_pair(inputIndex, prevTx));
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

            if (wtx.outputs.empty() && wtx.inputToWTX.empty()) {
                // no connection to our UTXOs
                if (--m_bloomScore < 25)
                    rebuildBloom();
                continue;
            }
            // remove UTXOs this Tx spent
            for (auto i = wtx.inputToWTX.begin(); i != wtx.inputToWTX.end(); ++i) {
                auto iter = m_unspentOutputs.find(i->second);
                assert(iter != m_unspentOutputs.end()); // a double spend? With checked merkle-block? Thats...odd.
                if (iter != m_unspentOutputs.end())
                    m_unspentOutputs.erase(iter);
            }

            for (auto i = wtx.outputs.begin(); i != wtx.outputs.end(); ++i) {
                uint64_t key = m_nextWalletTransactionId;
                key <<= 16;
                key += i->first;
                logDebug() << "   inserting output"<< i->first << Log::Hex << i->second.walletSecretId << key;
                m_unspentOutputs.insert(std::make_pair(key, i->second.value));
            }
            m_txidCash.insert(std::make_pair(wtx.txid, m_nextWalletTransactionId));
            m_walletTransactions.insert(std::make_pair(m_nextWalletTransactionId++, wtx));
            m_walletChanged = true;

            logCritical() << "Wallet" << m_segment->segmentId() << "claims" << tx.createHash() << "@" << blockHeight;
            transactionsToSave.push_back(tx);
        }
        assert(m_nextWalletTransactionId - firstNewTransaction == int(transactionsToSave.size()));
    }
    if (!transactionsToSave.empty()) {
        emit utxosChanged();
        emit appendedTransactions(firstNewTransaction, transactionsToSave.size());
    }

    for (auto tx : transactionsToSave) { // save the Tx to disk.
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
        }
    }
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
        m_segment->addToFilter(priv.second.address, priv.second.initialHeight);
    }
    for (auto utxo : m_unspentOutputs) {
        uint64_t key = utxo.first;
        uint32_t outputIndex = key & 0xFFFF;
        key >>= 16;
        assert(key < INT_MAX);
        assert(m_walletTransactions.find(int(key)) != m_walletTransactions.end());
        auto tx = m_walletTransactions.at(int(key));
        m_segment->addToFilter(tx.txid, outputIndex);
    }
    m_bloomScore = 0;
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

void Wallet::newTransaction(const Tx &tx)
{
    QMutexLocker locker(&m_lock);
    // TODO
    Q_UNUSED(tx);
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

    m_segment->addToFilter(pubkey.GetID(), currentBlockheight);

    rebuildBloom();
}

bool Wallet::addPrivateKey(const QString &privKey, int startBlockHeight)
{
    QMutexLocker locker(&m_lock);
    CBase58Data encodedData;
    auto bytes = privKey.toLatin1();
    encodedData.SetString(bytes.constData());
    if (encodedData.isMainnetPrivKey()) {
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

        m_segment->addToFilter(pubkey.GetID(), startBlockHeight);

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
        return;
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
    auto data = builder.buffer();

    try {
        boost::filesystem::create_directories(m_basedir);
        boost::filesystem::remove(m_basedir / "secrets.dat~");

        std::ofstream outFile((m_basedir / "secrets.dat~").string());
        outFile.write(data.begin(), data.size());

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
                assert(i->second.walletSecretId < INT_MAX);
                uint64_t key = index;
                key <<= 16;
                key += i->first;
                m_unspentOutputs.insert(std::make_pair(key, i->second.value));
            }
            newTx.insert(index);

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
        else if (parser.tag() == InputSpendsTx) {
            tmp = parser.intData();
        }
        else if (parser.tag() == InputSpendsOutput) {
            assert(inputIndex >= 0);
            assert(tmp != 0);
            uint64_t key = tmp;
            key <<= 16;
            key += parser.longData();
            wtx.inputToWTX.insert(std::make_pair(inputIndex, key));
            tmp = 0;
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

        // remove UTXOs this Tx spent
        for (auto i = iter->second.inputToWTX.begin(); i != iter->second.inputToWTX.end(); ++i) {
            auto utxo = m_unspentOutputs.find(i->second);
            assert(utxo != m_unspentOutputs.end()); // Loading should be done in order to avoid this.
            if (utxo != m_unspentOutputs.end())
                m_unspentOutputs.erase(utxo);
        }
    }

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
