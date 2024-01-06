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

#include "WalletEnums.h"

#include <p2p/P2PNetInterface.h>
#include <p2p/PeerAddressDB.h>

#include <QAbstractItemModel>
#include <QMutex>
#include <QTimer>

#include <vector>
#include <memory>

class QTimer;

class BasicAddressStats : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int count READ count CONSTANT FINAL)
    Q_PROPERTY(int banned READ banned CONSTANT FINAL)
    Q_PROPERTY(int partialBanned READ partialBanned CONSTANT FINAL)
    Q_PROPERTY(int ipv6Addresses READ ipv6Addresses CONSTANT FINAL)
    Q_PROPERTY(int everConnected READ everConnected CONSTANT FINAL)
    Q_PROPERTY(bool usesIPv4 READ usesIPv4 CONSTANT FINAL)
    Q_PROPERTY(bool usesIPv6 READ usesIPv6 CONSTANT FINAL)
public:
    explicit BasicAddressStats(const AddressDBStats &stats, QObject *parent = nullptr);

    int count() const;
    int banned() const;
    int partialBanned() const;
    int ipv6Addresses() const;
    int everConnected() const;
    bool usesIPv4() const;
    bool usesIPv6() const;

private:
    const int m_count;
    const int m_banned;
    const int m_partialBanned;
    const int m_ipv6Addresses;
    const int m_everConnected;
    const bool m_usesIPv4;
    const bool m_usesIPv6;
};

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

    Q_INVOKABLE QObject *createStats(QObject *parent) const;

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
