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
#include "NetDataProvider.h"

#include <QThread>

NetDataProvider::NetDataProvider(int initialBlockHeight, QObject *parent)
    : QObject(parent),
      m_blockHeight(initialBlockHeight)
{
    connect (this, SIGNAL(peerDeleted(int)), this, SLOT(deleteNetPeer(int)), Qt::QueuedConnection);
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
}

void NetDataProvider::lostPeer(int peerId)
{
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

void NetDataProvider::blockchainHeightChanged(int newHeight)
{
    m_blockHeight.storeRelease(newHeight);
    emit blockHeightChanged();
}

void NetDataProvider::punishMentChanged(int peerId)
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

QList<NetPeer *> NetDataProvider::peers() const
{
    return m_peers;
}

int NetDataProvider::blockheight() const
{
    return m_blockHeight.loadAcquire();
}
