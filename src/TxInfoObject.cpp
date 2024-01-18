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
#include "TxInfoObject.h"

#include "Wallet.h"
#include "Payment.h"

#include <QThread>
#include <QTimer>

#include <chrono>

constexpr int SECTION = 10004;

TxInfoObject::TxInfoObject(Payment *payment, const Tx &tx)
    : BroadcastTxData(tx),
    m_wallet(payment->wallet())
{
    assert(payment);
    assert(m_wallet);
    auto iter = m_wallet->m_txidCache.find(hash());
    assert(iter != m_wallet->m_txidCache.end());
    m_txIndex = iter->second;

    connect(this, SIGNAL(finished(int,bool)),
            m_wallet, SLOT(broadcastTxFinished(int,bool)), Qt::QueuedConnection);
}

TxInfoObject::TxInfoObject(Wallet *wallet, int txIndex, const Tx &tx)
    : BroadcastTxData(tx),
    m_wallet(wallet),
    m_txIndex(txIndex)
{
    connect(this, SIGNAL(finished(int,bool)),
            m_wallet, SLOT(broadcastTxFinished(int,bool)), Qt::QueuedConnection);
}

void TxInfoObject::txRejected(int connectionId, RejectReason reason, const std::string &message)
{
    // reason is hinted using BroadcastTxData::RejectReason
    logCritical(SECTION) << "Transaction rejected" << reason << message;

    /*
     * This is called directly from a peer's network processing code. Each peer may
     * be on a different 'strand' (effectively a thread) as a result of using NetworkManager,
     * and thus this method needs to be thread-safe.
     */
    QMutexLocker l(&m_lock);
    for (auto iter = m_broadcasts.begin(); iter != m_broadcasts.end(); ++iter) {
        if (iter->connectionId == connectionId) {
            assert(iter->failed == false); // the p2p lib should avoid this
            iter->failed = true;
            iter->rejected = reason;
            iter->rejectMsg = message;
            break;
        }
    }
    emit rejectionSeen();
}

uint16_t TxInfoObject::privSegment() const
{
    assert(m_wallet);
    assert(m_wallet->segment());
    return m_wallet->segment()->segmentId();
}

int TxInfoObject::txIndex() const
{
    return m_txIndex;
}

TxInfoObject::Status TxInfoObject::calcStatus(int waitingTimeoutMS) const
{
    Status rc;
    auto copy = m_broadcasts;
    rc.sent = copy.size();
    rc.inProgress = 0;

    const auto time = std::chrono::system_clock::now();
    const auto milliTime = std::chrono::duration_cast<std::chrono::milliseconds>(time.time_since_epoch());
    const uint64_t cutoff = milliTime.count() - waitingTimeoutMS;

    for (const auto &broadcast : copy) {
        if (broadcast.failed)
            rc.failed++;
        else if (broadcast.sentTime > cutoff)
            rc.inProgress++;
    }
    return rc;
}

/*
 * The broadcast happens in INV style, and we wait for the peer to then get the data,
 * which we notice with the 'sentOne' call. Called once per peer.
 * But likely only one or maybe two actually call this as the more likely scenario
 * (especially on slower networks) is that they get the transaction from each other.
 *
 * Except when the transaction is bad, and we get a rejected message (but only once)
 * and naturally that once is from each peer.
 * And that is where this method is most useful, to detect that the network rejects
 * our transaction.
 */
void TxInfoObject::checkState()
{
    assert(thread() == QThread::currentThread());
    Status status = calcStatus(3000);
    logDebug(SECTION) << "sent:" << status.sent << "in progress:" << status.inProgress << "failed:" << status.failed;
    // a typical wallet has 3 peers.
    // we react when at least 2 downloaded our transaction and didn't report a failure
    if (status.sent - status.inProgress >= 2) {
        emit finished(m_txIndex,
            /* bool success = */ status.sent - status.inProgress - status.failed >= 1);
    }
}

void TxInfoObject::sentVia(const std::shared_ptr<Peer> &peer)
{
    assert(peer.get());
     /*
     * This is called directly from a peer's network processing code. Each peer may
     * be on a different 'strand' (effectively a thread) as a result of using NetworkManager,
     * and thus this method needs to be thread-safe.
     */
     QMutexLocker l(&m_lock);
     const auto id = peer->connectionId();
     for (auto iter = m_broadcasts.begin(); iter != m_broadcasts.end(); ++iter) {
        if (iter->connectionId == id)
            return;
     }

    Broadcast broadcast;
    broadcast.connectionId = id;
    broadcast.ep = peer->peerAddress().peerAddress();
    const auto time = std::chrono::system_clock::now();
    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(time.time_since_epoch());
    broadcast.sentTime = millis.count();
    m_broadcasts.append(broadcast);

    if (m_broadcasts.size() >= 2) {
        // Two stage singleshots. First (Qt requires that to be zero ms) to move
        // to a thread that Qt owns.
        // Second (in the lambda) to actually wait several seconds.
        QTimer::singleShot(0, this, [=]() {
            assert(thread() == QThread::currentThread());
            QTimer::singleShot(4 * 1000, this, SLOT(checkState()));
        });
    }

    emit sentOne();
}
