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
#include "FloweePay.h"
#include "NetDataProvider.h"
#include <ConnectionManager.h>
#include <Peer.h>
#include <PrivacySegment.h>

#include <QThread>
#include <QTimer>

BasicAddressStats::BasicAddressStats(const AddressDBStats &stats, QObject *parent)
    : QObject(parent),
    m_count(stats.count),
    m_banned(stats.banned),
    m_partialBanned(stats.partialBanned),
    m_ipv6Addresses(stats.ipv6Addresses),
    m_everConnected(stats.everConnected),
    m_usesIPv4(stats.usesIPv4),
    m_usesIPv6(stats.usesIPv6)
{
}

int BasicAddressStats::count() const
{
    return m_count;
}

void BasicAddressStats::setCount(int newCount)
{
    if (m_count == newCount)
        return;
    m_count = newCount;
    emit countChanged();
}

int BasicAddressStats::banned() const
{
    return m_banned;
}

void BasicAddressStats::setBanned(int newBanned)
{
    if (m_banned == newBanned)
        return;
    m_banned = newBanned;
    emit bannedChanged();
}

int BasicAddressStats::partialBanned() const
{
    return m_partialBanned;
}

void BasicAddressStats::setPartialBanned(int newPartialBanned)
{
    if (m_partialBanned == newPartialBanned)
        return;
    m_partialBanned = newPartialBanned;
    emit partialBannedChanged();
}

int BasicAddressStats::ipv6Addresses() const
{
    return m_ipv6Addresses;
}

void BasicAddressStats::setIpv6Addresses(int newIpv6Addresses)
{
    if (m_ipv6Addresses == newIpv6Addresses)
        return;
    m_ipv6Addresses = newIpv6Addresses;
    emit ipv6AddressesChanged();
}

bool BasicAddressStats::usesIPv4() const
{
    return m_usesIPv4;
}

void BasicAddressStats::setUsesIPv4(bool newUsesIPv4)
{
    if (m_usesIPv4 == newUsesIPv4)
        return;
    m_usesIPv4 = newUsesIPv4;
    emit usesIPv4Changed();
}

bool BasicAddressStats::usesIPv6() const
{
    return m_usesIPv6;
}

void BasicAddressStats::setUsesIPv6(bool newUsesIPv6)
{
    if (m_usesIPv6 == newUsesIPv6)
        return;
    m_usesIPv6 = newUsesIPv6;
    emit usesIPv6Changed();
}

int BasicAddressStats::everConnected() const
{
    return m_everConnected;
}

void BasicAddressStats::setEverConnected(int newEverConnected)
{
    if (m_everConnected == newEverConnected)
        return;
    m_everConnected = newEverConnected;
    emit everConnectedChanged();
}


// ////////////////////////////////////////////////////////////////////////////////////

NetDataProvider::NetDataProvider(QObject *parent)
    : QAbstractListModel(parent)
{
    m_refreshTimer.setInterval(1200);
    m_refreshTimer.setTimerType(Qt::VeryCoarseTimer);
    connect (&m_refreshTimer, SIGNAL(timeout()), this, SLOT(updatePeers()));
}

void NetDataProvider::startTimer()
{
    m_refreshTimer.start();
}

void NetDataProvider::newConnection(const std::shared_ptr<Peer> &peer)
{
    QMutexLocker<QRecursiveMutex> l(&m_peerMutex);
    beginInsertRows(QModelIndex(), m_peers.size(), m_peers.size());
    PeerData pd;
    pd.peer = peer;
    m_peers.push_back(pd);
    endInsertRows();
}

int NetDataProvider::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) // only for the (invalid) root node we return a count, since this is a list not a tree
        return 0;
    QMutexLocker<QRecursiveMutex> l(&m_peerMutex);
    return m_peers.size();
}

QVariant NetDataProvider::data(const QModelIndex &index_, int role) const
{
    if (!index_.isValid())
        return QVariant();

    QMutexLocker<QRecursiveMutex> l(&m_peerMutex);
    const auto index = index_.row();
    if (index < 0 || index >= m_peers.size())
        return QVariant();

    const auto &peerData = m_peers.at(index);
    std::shared_ptr<Peer> peer = peerData.peer.lock();
    if (peer.get() == nullptr)
        return QVariant();

    switch (role) {
    case ConnectionId:
        return QVariant::fromValue<int>(peer->connectionId());
    case UserAgent:
        return QVariant::fromValue<QString>(QString::fromStdString(peer->userAgent()));
    case StartHeight:
        return QVariant::fromValue<int>(peer->startHeight());
    case NodeValidity:
        return QVariant::fromValue<WalletEnums::PeerValidity>(peerData.valid);
    case Address: {
        QString answer;
        const auto &ep = peer->peerAddress().peerAddress();
        if (ep.ipAddress.is_unspecified())
            answer = QString::fromStdString(ep.hostname);
        else
            answer = QString::fromStdString(ep.ipAddress.to_string());
        answer += QString(":%1").arg(ep.announcePort);
        return QVariant(answer);
    }
    case SegmentId:
        return QVariant::fromValue<int>(peerData.segment);
    case Downloading:
        return QVariant::fromValue<bool>(peerData.isDownloading);
    case PeerHeight:
        return QVariant::fromValue<int>(peer->peerHeight());
    case BanScore:
        return QVariant::fromValue<short>(peer->peerAddress().punishment());
    }

    return QVariant();
}

QHash<int, QByteArray> NetDataProvider::roleNames() const
{
    QHash<int, QByteArray> mapping;
    mapping[ConnectionId] = "connectionId";
    mapping[UserAgent] = "userAgent";
    mapping[StartHeight] = "startHeight";
    mapping[NodeValidity] = "validity";
    mapping[Address] = "address";
    mapping[SegmentId] = "segment";
    mapping[Downloading] = "isDownloading";
    mapping[StartHeight] = "startHeight";
    mapping[PeerHeight] = "height";
    mapping[BanScore] = "banScore";
    return mapping;
}

QObject *NetDataProvider::createStats(QObject *parent) const
{
    return new BasicAddressStats(
        FloweePay::instance()->p2pNet()->connectionManager().peerAddressDb().createStats(),
        parent);
}

void NetDataProvider::pardonBanned()
{
    FloweePay::instance()->p2pNet()->connectionManager().peerAddressDb().resetAllStats();
}

void NetDataProvider::disconnectPeer(int connectionId)
{
    FloweePay::instance()->p2pNet()->connectionManager().disconnect(connectionId);
}

void NetDataProvider::banPeer(int connectionId)
{
    FloweePay::instance()->p2pNet()->connectionManager().punish(connectionId, 1000);
}

void NetDataProvider::updatePeers()
{
    /*
     * The peers are managed by the p2p layer and they don't send any events on
     * changes, so we just do polling since that's cheap enough. An average wallet
     * will not have even 100 connections, making this check very cheap.
     *
     * One of the main things is that a deleted peer needs to be removed from
     * the list and we have some properties that may change over time (like
     * its validation state) which we check for.
     */
    QMutexLocker<QRecursiveMutex> l(&m_peerMutex);
    int index = 0;
    auto iter = m_peers.begin();
    while (iter != m_peers.end()) {
        auto peer = iter->peer.lock();
        if (peer.get() == nullptr) {
            // the peer has been deleted.
            beginRemoveRows(QModelIndex(), index, index);
            iter = m_peers.erase(iter);
            endRemoveRows();
            continue;
        }
        bool changed = false;

        WalletEnums::PeerValidity valid = WalletEnums::OpeningConnection;
        bool isDownloading = false;
        if (peer->status() == Peer::Connected) {
            const auto address = peer->peerAddress();
            if (address.hasEverConnected()) {
                // p2p level handshake some time in the past successeded
                valid = WalletEnums::KnownGood;
            }
            if (peer->requestedHeader())
                valid = peer->receivedHeaders() ? WalletEnums::CheckedOk : WalletEnums::Checking;
            isDownloading = peer->merkleDownloadInProgress();
        }
        if (valid != iter->valid) {
            iter->valid = valid;
            changed = true;
        }

        if (iter->isDownloading != isDownloading) {
            iter->isDownloading = isDownloading;
            changed = true;
        }

        if (peer->status() == Peer::Connected && iter->segment == 0) {
            auto segment = peer->privacySegment();
            if (segment) {
                iter->segment = segment->segmentId();
                changed = true;
            }
        }

        if (changed) {
            // to change a row, we delete and re-insert it.
            beginRemoveRows(QModelIndex(), index, index);
            endRemoveRows();
            beginInsertRows(QModelIndex(), index, index);
            endInsertRows();
        }
        ++index;
        ++iter;
    }
}

int NetDataProvider::selectedId() const
{
    return m_selectedId;
}

void NetDataProvider::setSelectedId(int newSelectedId)
{
    QMutexLocker<QRecursiveMutex> l(&m_peerMutex);
    if (m_selectedId == newSelectedId)
        return;
    m_selectedId = newSelectedId;
    emit selectedIdChanged();
}
