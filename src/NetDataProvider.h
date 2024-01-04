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
#ifndef NETDATAPROVIDER_H
#define NETDATAPROVIDER_H

#include <p2p/P2PNetInterface.h>
#include "WalletEnums.h"

#include <QAbstractItemModel>
#include <QMutex>
#include <QTimer>

#include <vector>
#include <memory>

class QTimer;

class NetDataProvider : public QAbstractListModel, public P2PNetInterface
{
    Q_OBJECT
public:
    explicit NetDataProvider(QObject *parent = nullptr);

    // the refresh data polling timer.
    void startTimer();

    // P2PNetInterface interface
    void newPeer(const std::shared_ptr<Peer> &peer) override;

    enum {
        ConnectionId = Qt::UserRole,
        UserAgent,
        StartHeight,
        NodeValidity,
        Address,
        SegmentId,
        Downloading,
        PeerHeight,
        BanScore
    };

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

private slots:
    void updatePeers();

private:
    mutable QRecursiveMutex m_peerMutex;
    struct PeerData {
        std::weak_ptr<Peer> peer;
        WalletEnums::PeerValidity valid = WalletEnums::UnknownValidity;
        bool isDownloading = false;
        uint16_t segment = 0;
    };

    std::vector<PeerData> m_peers;

    // we use some polling to keep the data up-to-date
    QTimer m_refreshTimer;
};

#endif
