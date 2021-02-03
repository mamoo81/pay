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
#ifndef NETDATAPROVIDER_H
#define NETDATAPROVIDER_H

#include "NetPeer.h"

#include <p2p/P2PNetInterface.h>

#include <QList>
#include <QMutex>

class NetDataProvider : public QObject, public P2PNetInterface
{
    Q_OBJECT
    Q_PROPERTY(QList<NetPeer*> peers READ peers NOTIFY peerListChanged)
    Q_PROPERTY(int blockheight READ blockheight NOTIFY blockHeightChanged)
public:
    explicit NetDataProvider(int initialBlockHeight, QObject *parent = nullptr);

    // P2PNetInterface
    void newPeer(int peerId, const std::string &userAgent, int startHeight, PeerAddress address) override;
    void lostPeer(int peerId) override;
    void blockchainHeightChanged(int newHeight) override;
    void punishMentChanged(int peerId) override;

    QList<NetPeer*> peers() const;
    int blockheight() const;

signals:
    void peerListChanged();
    void blockHeightChanged();
    void peerDeleted(int id);

private slots:
    void deleteNetPeer(int id);

private:
    mutable QMutex m_peerMutex;
    QList<NetPeer*> m_peers;
    QAtomicInt m_blockHeight;
};

#endif
