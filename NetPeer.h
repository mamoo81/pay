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
#ifndef PAY_PEER_H
#define PAY_PEER_H

#include <PeerAddressDB.h>
#include <QObject>

class NetPeer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString userAgent READ userAgent CONSTANT)
    Q_PROPERTY(int startHeight READ startHeight CONSTANT)
    Q_PROPERTY(int banScore READ banScore NOTIFY banScoreChanged)
    Q_PROPERTY(QString address READ address CONSTANT)
    Q_PROPERTY(int segmentId READ segmentId CONSTANT)
    Q_PROPERTY(QStringList services READ services CONSTANT)
    Q_PROPERTY(bool headersReceived READ headersReceived NOTIFY headersReceivedChanged)
    Q_PROPERTY(bool relaysTransactions READ relaysTransactions NOTIFY relaysTransactionsChanged)
public:
    NetPeer(int connectionId, const QString &userAgent, int startHeight, PeerAddress address, QObject *parent = nullptr);

    QString userAgent() const;

    int connectionId() const;
    int startHeight() const;
    int banScore() const;
    /// the ID that matches us with a certain wallet
    uint16_t segmentId() const;

    QString address() const; // ip address, as string

    void notifyPunishmentChanged();

    void setHeadersReceived(bool received);
    bool headersReceived() const;

    bool relaysTransactions() const;
    void setRelaysTransactions(bool newRelaysTransactions);

    QStringList services() const;
    void setServices(uint64_t services);

signals:
    void banScoreChanged();
    void headersReceivedChanged();
    void relaysTransactionsChanged();
    void servicesChanged();

private:
    const QString m_userAgent;
    const int m_connectionId;
    const int m_startHeight;
    PeerAddress m_address;

    bool m_headersReceived = false;
    bool m_relaysTransactions = false;
    uint64_t m_services = 0;
};

#endif
