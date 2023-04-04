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
#include <QString>
#include "Wallet.h"
#include "Wallet_p.h"
#include "FloweePay.h"
#include "TransactionInfo.h"
#include <cashaddr.h>

#include <QTimer>
#include <QThread>

#include <primitives/script.h>
#include <streaming/BufferPools.h>


namespace {
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

QString renderAddress(const Streaming::ConstBuffer &outputScript)
{
    std::vector<std::vector<uint8_t> > vSolutions;
    Script::TxnOutType whichType;
    if (!Script::solver(outputScript, whichType, vSolutions))
        return QString();

    switch (whichType) {
    case Script::TX_PUBKEY:
        return renderAddress(PublicKey(vSolutions[0]).getKeyId());
    case Script::TX_PUBKEYHASH:
        return renderAddress(KeyId(uint160(vSolutions[0])));
    case Script::TX_SCRIPTHASH: {
        CashAddress::Content c;
        c.type = CashAddress::SCRIPT_TYPE;
        c.hash = vSolutions[0];
        auto s = CashAddress::encodeCashAddr(chainPrefix(), c);
        const auto size = chainPrefix().size();
        return QString::fromLatin1(s.c_str() + size + 1, s.size() - size -1); // the 1 is for the colon
    }
    default:
        return QString();
    }
}
} // anon namespace

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

Wallet::OutputRef::OutputRef(uint64_t encoded)
{
    m_outputIndex = encoded & 0xFFFF;
    encoded >>= 16;
    assert(encoded < 0xFFFFFFFF);
    m_txid = encoded & 0x7FFFFFFF;
    assert(static_cast<int64_t>(m_txid) >= 0);
}

Wallet::OutputRef::OutputRef(int txIndex, int outputIndex)
    : m_txid(txIndex),
      m_outputIndex(outputIndex)
{
    assert(txIndex >= 0); // zero is the 'invalid' state, which is allowed here to make an invalid OutputRef
    assert(outputIndex >= 0);
    assert(outputIndex <= 0XFFFF);
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
        // Two stage singleshots. First (Qt requires that to be zero ms) to move
        // to a thread that Qt owns.
        // Second (in startCheckState()) to actually wait several seconds.
        QTimer::singleShot(0, this, SLOT(startCheckState()));
    }
}

void WalletInfoObject::startCheckState()
{
    Q_ASSERT(thread() == QThread::currentThread());
    QTimer::singleShot(5 * 1000, this, SLOT(checkState()));
}

void WalletInfoObject::checkState()
{
    Q_ASSERT(thread() == QThread::currentThread());
    if (m_sentPeerCount >= 2) {
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


// //////////////////////////////////////////////////

bool Wallet::WalletTransaction::isUnconfirmed() const
{
    return minedBlockHeight == WalletPriv::Unconfirmed;
}

bool Wallet::WalletTransaction::isRejected() const
{
    return minedBlockHeight == WalletPriv::Rejected;
}


// //////////////////////////////////////////////////

void Wallet::fetchTransactionInfo(TransactionInfo *info, int txIndex)
{
    Q_ASSERT(info);
    QMutexLocker locker(&m_lock);
    auto iter = m_walletTransactions.find(txIndex);
    if (m_walletTransactions.end() == iter)
        throw std::runtime_error("Invalid tx-index");

    const auto &wtx = iter->second;
    info->m_isCashFusion = wtx.isCashFusionTx;
    info->m_isCoinbase = wtx.isCoinbase;
    info->m_userComment = wtx.userComment;

    Tx tx = loadTransaction(wtx.txid, Streaming::pool(0));
    info->m_txSize = tx.size();

    /*
     * The QML to show these lists of inputs and outputs depends on
     * the quality of the column layout component. It turns out that
     * in an older version of Qt this simple didn't work and all items
     * are shown on top of each other.
     * This "fixes" that problem by simply not generating the data
     * to show in the UI.
     * Known problem was in Ubuntu 20.04 (LTS) with Qt 5.12.8
     */
    const auto qtVersion = QString(qVersion()).split('.');
    assert(qtVersion.size() == 3);
    if (qtVersion.at(0).toInt() <= 5 && qtVersion.at(1).toInt() < 15)
        return;

    // find out how many inputs and how many outputs there are.
    Tx::Iterator txIter(tx);
    // If we created this transaction (we have inputs in it anyway) then
    // also look up all the outputs from the file.
    // This creates a false-positive for tx we co-created (flipstarter etc), we already
    // check for cash-fusion.
    const bool createdByUs = !wtx.inputToWTX.empty() && !wtx.isCashFusionTx;
    info->m_createdByUs = createdByUs;
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
    for (auto pair : wtx.inputToWTX) {
        OutputRef ref(pair.second);
        auto w = m_walletTransactions.find(ref.txIndex());
        assert(w != m_walletTransactions.end());
        auto prevOut = w->second.outputs.find(ref.outputIndex());
        assert(prevOut != w->second.outputs.end());

        auto in = new TransactionInputInfo(info);
        in->setValue(prevOut->second.value);
        auto secretIter = m_walletSecrets.find(prevOut->second.walletSecretId);
        assert(secretIter != m_walletSecrets.end());
        const auto &secret = secretIter->second;
        in->setAddress(renderAddress(secret.address));
        if (secret.fromChangeChain) {
            assert(secret.fromHdWallet);
            in->setCloakedAddress(tr("Change #%1").arg(secret.hdDerivationIndex));
        }
        info->m_inputs[pair.first] = in;
    }
    // same for outputs
    for (auto o : wtx.outputs) {
        auto secretIter = m_walletSecrets.find(o.second.walletSecretId);
        assert(secretIter != m_walletSecrets.end());
        const auto &secret = secretIter->second;
        TransactionOutputInfo *out;
        if (createdByUs) { // reuse the one we created before from the raw Td.
            out = info->m_outputs[o.first];
            out->setForMe(true);
        }
        else {
            out = new TransactionOutputInfo(info);
            out->setValue(o.second.value);
            out->setAddress(renderAddress(secret.address));
        }
        out->setSpent(m_unspentOutputs.find(OutputRef(txIndex, o.first).encoded()) == m_unspentOutputs.end());
        if (secret.fromChangeChain) {
            assert(secret.fromHdWallet);
            out->setCloakedAddress(tr("Change #%1").arg(secret.hdDerivationIndex));
        }
        info->m_outputs[o.first] = out;
    }
}

// Apologies for the nested loop design, I promise this is not representative code and should only ever be ran once per wallet, ever.
void Wallet::populateSigType()
{
    const auto &txs = m_walletTransactions; // short alias for readability

    logCritical().nospace() << "Upgrading wallet '" << m_name << "', Finding signature types from seen transactions";
    // iterate though each private key
    for (auto s = m_walletSecrets.begin(); s != m_walletSecrets.end(); ++s) {
        auto &secret = s->second;
        // logDebug() << "Secret" << s->first << "Hd:" << secret.fromHdWallet << secret.fromChangeChain << "index:" << secret.hdDerivationIndex;
        // iterate through transactions and outputs to find one that deposited funds there.
        for (auto t1 = txs.cbegin(); secret.signatureType == NotUsedYet && t1 != txs.cend(); ++t1) {
            const auto &tx1 = t1->second;
            for (auto o = tx1.outputs.cbegin(); secret.signatureType == NotUsedYet && o != tx1.outputs.cend(); ++o) {
                if (o->second.walletSecretId == s->first) {
                    // logDebug() << " Found an out for secret" << t1->first << o->first;
                    const auto ref = OutputRef(t1->first, o->first).encoded();
                    // check UTXO, if still there, then its unspent.
                    // Unspent means no signature, so find another output to check.
                    if (m_unspentOutputs.find(ref) != m_unspentOutputs.end())
                        continue;

                    // iterate through transactions and inputs and try to find the spending input.
                    for (auto t2 = txs.cbegin(); secret.signatureType == NotUsedYet && t2  != txs.cend(); ++t2) {
                        const auto &tx2 = t2->second;
                        for (auto i = tx2.inputToWTX.cbegin(); i != tx2.inputToWTX.cend(); ++i) {
                            if (i->second == ref) {
                                // found one, now fetch the input script and check the type.
                                // logDebug() << "Found tx2 which spends output. Tx2:" << t2->first << i->first;
                                Tx tx = loadTransaction(tx2.txid, Streaming::pool(0));
                                Tx::Iterator txIter(tx);
                                for (int x = i->first; x >= 0; --x) {
                                    if (txIter.next(Tx::TxInScript) != Tx::TxInScript) {
                                        logCritical() << "Internal error; tx has too little inputs" << tx2.txid << i->first;
                                        return;
                                    }
                                }
                                assert(txIter.tag() == Tx::TxInScript);
                                CScript script(txIter.byteData());
                                auto scriptIter = script.begin();
                                opcodetype type;
                                script.GetOp(scriptIter, type);
                                secret.signatureType = type == 65 ? SignedAsSchnorr : SignedAsEcdsa;
                                logInfo() << "Secret key" << s->first << "detected signature type from tx-out:"
                                           << tx1.txid << o->first
                                           << "signed by tx-input:" << tx2.txid << i->first
                                           << "SigType:" << (type == 65 ? "SignedAsSchnorr" : "SignedAsEcdsa");
                                m_secretsChanged = true;
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
    logCritical() << "Wallet upgrade finished";
}


// //////////////////////////////////////////////////

Wallet::InsertBeforeData::InsertBeforeData(Wallet *wallet)
    : parent(wallet)
{
}

Wallet::InsertBeforeData::~InsertBeforeData()
{
    if (transactions.empty())
        return;

    // we proceed to call 'addTransactions' for each block
    parent->m_inInsertBeforeCallback = true;
    int blockHeight = 0;
    uint256 blockId;
    std::deque<Tx> list;
    for (auto wtx = transactions.rbegin(); wtx != transactions.rend(); ++wtx) {
        if (wtx->minedBlockHeight != blockHeight) {
            if (!list.empty())
                parent->newTransactions(blockId, blockHeight, list);
            blockId = wtx->minedBlock;
            blockHeight = wtx->minedBlockHeight;
            list = std::deque<Tx>();
        }

        // we need to re-load the tx from disk
        auto tx = parent->loadTransaction(wtx->txid, Streaming::pool(0));
        assert(tx.isValid());
        list.push_back(tx);
    }
    if (!list.empty())
        parent->newTransactions(blockId, blockHeight, list);
    parent->m_inInsertBeforeCallback = true;
}

Wallet::InsertBeforeData Wallet::removeTransactionsAfter(int blockHeight)
{
    InsertBeforeData ibd(this);
    if (m_inInsertBeforeCallback)
        return ibd;

    int lastToRemove = m_nextWalletTransactionId;
    int txIndex = -1;
    assert(m_walletTransactions.find(lastToRemove) == m_walletTransactions.end());
    for (auto i = m_walletTransactions.rbegin(); i != m_walletTransactions.rend(); ++i) { // backwards iterator
        if (i->second.minedBlockHeight <= blockHeight) {
            // avoid walking the entire chain, since they are in-order we can just abort here.
            break;
        }
        lastToRemove = i->first;
        if (txIndex == -1) // remember first to remove.
            txIndex = i->first;
    }
    assert (lastToRemove <= m_nextWalletTransactionId);

    // we do this in two loops because a map reverse iterator doesn't have a matching 'erase'
    // method.
    // Using the 'key' in the next loop instead.

    while (txIndex >= lastToRemove) {
        auto i = m_walletTransactions.find(txIndex--);
        assert (i != m_walletTransactions.end());
        // this transaction was appended to the UTXO and now something has
        // to be inserted before, which may alter the UTXO to such an extend that THIS
        // transaction may be found to spend more outputs.
        //
        // first, re-add the utxo's that were removed in initial add.
        const auto &wtx = i->second;
        for (auto input : wtx.inputToWTX) {
            OutputRef ref(input.second);
            // the unspent outputs struct holds the value, which means we need to look that up again.
            auto prev = m_walletTransactions.find(ref.txIndex());
            assert(prev != m_walletTransactions.end());
            const auto &outputs = prev->second.outputs;
            auto prevOut = outputs.find(ref.outputIndex());
            assert(prevOut != outputs.end());
            m_unspentOutputs.insert(std::make_pair(input.second, prevOut->second.value));
        }

        // second, remove the UTXOs this transaction added.
        for (auto o = wtx.outputs.begin(); o != wtx.outputs.end(); ++o) {
            const auto key = OutputRef(i->first, o->first).encoded();
            auto utxo = m_unspentOutputs.find(key);
            assert(utxo != m_unspentOutputs.end());
            m_unspentOutputs.erase(utxo);

            // things like coinbases may get a lock record, lets remove one if it exists.
            auto lock = m_lockedOutputs.find(key);
            if (lock != m_lockedOutputs.end()) {
                m_lockedOutputs.erase(lock);
            }
        }

        ibd.transactions.push_back(i->second);
        m_walletTransactions.erase(i);
        auto txidIter = m_txidCache.find(wtx.txid);
        assert(txidIter != m_txidCache.end());
        m_txidCache.erase(txidIter);
    }
    return ibd;
}
