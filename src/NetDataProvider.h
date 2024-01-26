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
    Q_PROPERTY(int count READ count WRITE setCount NOTIFY countChanged FINAL)
    Q_PROPERTY(int banned READ banned WRITE setBanned NOTIFY bannedChanged FINAL)
    Q_PROPERTY(int partialBanned READ partialBanned WRITE setPartialBanned NOTIFY partialBannedChanged FINAL)
    Q_PROPERTY(int ipv6Addresses READ ipv6Addresses WRITE setIpv6Addresses NOTIFY ipv6AddressesChanged FINAL)
    Q_PROPERTY(int everConnected READ everConnected WRITE setEverConnected NOTIFY everConnectedChanged FINAL)
    Q_PROPERTY(bool usesIPv4 READ usesIPv4 WRITE setUsesIPv4 NOTIFY usesIPv4Changed FINAL)
    Q_PROPERTY(bool usesIPv6 READ usesIPv6 WRITE setUsesIPv6 NOTIFY usesIPv6Changed FINAL)
public:
    explicit BasicAddressStats(const AddressDBStats &stats, QObject *parent = nullptr);

    int count() const;
    void setCount(int newCount);

    int banned() const;
    void setBanned(int newBanned);

    int partialBanned() const;
    void setPartialBanned(int newPartialBanned);

    int ipv6Addresses() const;
    void setIpv6Addresses(int newIpv6Addresses);

    int everConnected() const;
    void setEverConnected(int newEverConnected);

    bool usesIPv4() const;
    void setUsesIPv4(bool newUsesIPv4);

    bool usesIPv6() const;
    void setUsesIPv6(bool newUsesIPv6);

signals:
    void countChanged();
    void bannedChanged();
    void partialBannedChanged();
    void ipv6AddressesChanged();
    void everConnectedChanged();
    void usesIPv4Changed();
    void usesIPv6Changed();

private:
    int m_count;
    int m_banned;
    int m_partialBanned;
    int m_ipv6Addresses;
    int m_everConnected;
    bool m_usesIPv4;
    bool m_usesIPv6;
};

class NetDataProvider : public QAbstractListModel, public P2PNetInterface
{
    Q_OBJECT
public:
    explicit NetDataProvider(QObject *parent = nullptr);

    // the refresh data polling timer.
    void startTimer();

    // P2PNetInterface interface
    void newConnection(const std::shared_ptr<Peer> &peer) override;

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
    Q_INVOKABLE void pardonBanned();

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
