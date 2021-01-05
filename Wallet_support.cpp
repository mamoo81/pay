/*
 * This file is part of the Flowee project
 * Copyright (C) 2020 Tom Zander <tom@flowee.org>
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

#include <QTimer>
#include <QThread>


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
