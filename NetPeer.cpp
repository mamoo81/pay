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
    return "TODO";
}

void NetPeer::notifyPunishmentChanged()
{
    emit banScoreChanged();
}
