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
#include <QString>
#include "Wallet.h"
#include "Wallet_p.h"
#include "FloweePay.h"
#include "TransactionInfo.h"
#include <cashaddr.h>

#include <QTimer>
#include <QThread>

#include <primitives/script.h>


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

    CKeyID keyID;
    switch (whichType)
    {
    case Script::TX_PUBKEY:
        keyID = CPubKey(vSolutions[0]).getKeyId();
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
    assert(m_txid >= 0);
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

Wallet::HierarchicallyDeterministicWalletData::HierarchicallyDeterministicWalletData(const std::string &seedWords, const std::string &pwd)
    : masterKey(HDMasterKey::fromMnemonic(seedWords, pwd))
{
    walletMnemonic = QString::fromUtf8(seedWords.c_str());
    walletMnemonicPwd = QString::fromUtf8(pwd.c_str());
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

    Tx tx = loadTransaction(wtx.txid, FloweePay::pool(0));
    info->m_txSize = tx.size();

    // find out how many inputs and how many outputs there are.
    Tx::Iterator txIter(tx);
    // If we created this transaction (we have inputs in it anyway) then
    // also look up all the outputs from the file.
    // TODO this creates a false-positive for tx we co-created (cashfusion, flipstarter etc)
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
                                Tx tx = loadTransaction(tx2.txid, FloweePay::pool(0));
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
