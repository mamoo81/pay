/*
 * This file is part of the Flowee project
 * Copyright (C) 2020-2023 Tom Zander <tom@flowee.org>
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
#include "NetDataProvider.h"
#include "NetPeer.h"
#include "FloweePay.h"
#include <ConnectionManager.h>
#include <Peer.h>

#include <QThread>
#include <QTimer>

NetDataProvider::NetDataProvider(QObject *parent)
    : QObject(parent)
{
    connect (this, SIGNAL(peerDeleted(int)), this, SLOT(deleteNetPeer(int)), Qt::QueuedConnection); // Make this thread-safe
}


void NetDataProvider::newPeer(int peerId, const std::string &userAgent, int startHeight, PeerAddress address)
{
    QMutexLocker l(&m_peerMutex);
    NetPeer *newPeer = new NetPeer(peerId, QString::fromStdString(userAgent), startHeight, address);
    // assume this interface method is called in a thread that is not the Qt main one.
    newPeer->moveToThread(thread());
    newPeer->setParent(this);
    m_peers.append(newPeer);
    emit peerListChanged();

    if (m_refreshTimer)
        QTimer::singleShot(0, m_refreshTimer, SLOT(start()));
}

void NetDataProvider::lostPeer(int peerId)
{
    // this callback is not guarenteed to be made in our thread.
    emit peerDeleted(peerId);
}

void NetDataProvider::deleteNetPeer(int peerId)
{
    Q_ASSERT(QThread::currentThread() == thread()); // make sure we have no threading issues
    QMutexLocker l(&m_peerMutex);
    QList<NetPeer *> peers(m_peers);
    for (int i = 0; i < peers.size(); ++i) {
        if (peers.at(i)->connectionId() == peerId) {
            auto oldPeer = peers.takeAt(i);
            m_peers = peers;
            emit peerListChanged();
            oldPeer->deleteLater();
            return;
        }
    }
}

void NetDataProvider::updatePeers()
{
    auto &conMan = FloweePay::instance()->p2pNet()->connectionManager();
    QList<NetPeer *> peers;
    {
        QMutexLocker l(&m_peerMutex);
        peers = m_peers;
    }
    bool stopTimer = true;
    for (auto &p : peers) {
        auto peer = conMan.peer(p->connectionId());
        // update 'p' with up to date data from the peer.
        p->setRelaysTransactions(peer->relaysTransactions());
        p->setHeadersReceived(peer->receivedHeaders());

        if (peer->privacySegment() == nullptr)
            stopTimer = false;
    }
    if (stopTimer)
        m_refreshTimer->stop();
}

void NetDataProvider::punishmentChanged(int peerId)
{
    QMutexLocker l(&m_peerMutex);
    QList<NetPeer *> peers(m_peers);
    for (auto &p : peers) {
        if (p->connectionId() == peerId) {
            p->notifyPunishmentChanged();
            break;
        }
    }
}

QList<QObject *> NetDataProvider::peers() const
{
    QList<QObject *> answer;
    for (auto * const p : m_peers) {
        answer.append(p);
    }
    return answer;
}

void NetDataProvider::startRefreshTimer()
{
    /*
     * Start a timer that checks every second if the peer has changed one of the not-broadcast status.
     * When a segment is assigned we can stop the timer.
     */
    QMutexLocker l(&m_peerMutex);
    assert(m_refreshTimer == nullptr); // can be called only once
    m_refreshTimer = new QTimer(this);
    connect(m_refreshTimer, SIGNAL(timeout()), this, SLOT(updatePeers()));
    m_refreshTimer->setTimerType(Qt::VeryCoarseTimer);
    m_refreshTimer->setInterval(950);
}
