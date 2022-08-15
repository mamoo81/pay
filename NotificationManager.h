/*
 * This file is part of the Flowee project
 * Copyright (C) 2021-2022 Tom Zander <tom@flowee.org>
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
#ifndef NOTIFICATIONMANAGER_H
#define NOTIFICATIONMANAGER_H

#include <NotificationListener.h>
#include <QDateTime>
#include <QObject>
#include <QVariantMap>

class QDBusMessage;
class QDBusInterface;

class NotificationManager : public QObject, public NotificationListener
{
    Q_OBJECT
public:
    explicit NotificationManager(QObject *parent = nullptr);

    void notifyNewBlock(const P2PNet::Notification &notification) override;
    void segmentUpdated(const P2PNet::Notification &) override;

    bool newBlockMuted() const;
    void setNewBlockMuted(bool mute);

signals:
    // the above will happen in the network thread, lets move them to our thread with some slots.
    void newBlockSeenSignal(int blockHeight);
    void segmentUpdatedSignal();
    void newBlockMutedChanged();

private slots:
    // the above will happen in the network thread, lets move them to our thread with some slots.
    void newBlockSeen(int blockHeight);
    void newBlockNotificationShown(uint id);
    void notificationClosed(uint id, uint subId);
    void actionInvoked(uint id, const QString &actionKey);

    void walletUpdated();
    void walletUpdateNotificationShown(uint id);

private:
    QDBusInterface *remote();

    bool m_newBlockMuted = false;
    bool m_openingNewFundsNotification = false;
    QDateTime m_startupTime;

#if QT_DBUS_LIB
    uint32_t m_blockNotificationId = 0;
    uint32_t m_newFundsNotificationId = 0;

    QVariantMap m_newBlockHints;
    QVariantMap m_walletUpdateHints;
    QDBusInterface *m_remote = nullptr;
#endif
};

#endif
