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
#include "NetPeer.h"

NetPeer::NetPeer(int connectionId, const QString &userAgent, int startHeight, PeerAddress address, QObject *parent)
    : QObject(parent),
    m_userAgent(userAgent),
    m_connectionId(connectionId),
    m_startHeight(startHeight),
    m_address(address)
{
}

QString NetPeer::userAgent() const
{
    return m_userAgent;
}

int NetPeer::connectionId() const
{
    return m_connectionId;
}

int NetPeer::startHeight() const
{
    return m_startHeight;
}

int NetPeer::banScore() const
{
    return m_address.punishment();
}

uint16_t NetPeer::segmentId() const
{
    return m_address.segment();
}

QString NetPeer::address() const
{
    QString answer;
    const auto &ep = m_address.peerAddress();
    if (ep.ipAddress.is_unspecified())
        answer = QString::fromStdString(ep.hostname);
    else
       answer = QString::fromStdString(ep.ipAddress.to_string());
    answer += QString(":%1").arg(ep.announcePort);
    return answer;
}

void NetPeer::notifyPunishmentChanged()
{
    emit banScoreChanged();
}

void NetPeer::setHeadersReceived(bool received)
{
    if (received == m_headersReceived)
        return;
    m_headersReceived = received;
    emit headersReceivedChanged();
}

bool NetPeer::headersReceived() const
{
    return m_headersReceived;
}

bool NetPeer::relaysTransactions() const
{
    return m_relaysTransactions;
}

void NetPeer::setRelaysTransactions(bool newRelaysTransactions)
{
    if (m_relaysTransactions == newRelaysTransactions)
        return;
    m_relaysTransactions = newRelaysTransactions;
    emit relaysTransactionsChanged();
}

QStringList NetPeer::services() const
{
    QStringList answer;
    if (m_services & 1)
        answer << QLatin1String("NETWORK");
    if (m_services & (1 << 2))
        answer << QLatin1String("BLOOM");
    if (m_services & (1 << 4))
        answer << QLatin1String("XTHIN");
    if (m_services & (1 << 5))
        answer << QLatin1String("CASH");
    if (m_services & (1 << 8))
        answer << QLatin1String("CF");
    if (m_services & (1 << 10))
        answer << QLatin1String("NETWORK_LIMITED");
    if (m_services & (1 << 11)) // obsolete, but still used
        answer << QLatin1String("EXTVERSION");
    return answer;
}

void NetPeer::setServices(uint64_t services)
{
    if (m_services == services)
        return;
    m_services = services;
    emit servicesChanged();
}
