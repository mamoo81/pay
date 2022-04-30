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
#include "NotificationManager.h"
#include "FloweePay.h"
#include "PriceDataProvider.h"
#include <utils/Logger.h>
#include <QDBusConnection>
#include <QDBusInterface>

#include <QTimer>

NotificationManager::NotificationManager(QObject *parent)
    : QObject(parent)
{
    setCollation(true);

    // We use the notification spec (v 1.2)
    // https://specifications.freedesktop.org/notification-spec/notification-spec-latest.html
    m_newBlockHints["desktop-entry"] = "org.flowee.pay";
    m_newBlockHints["urgency"] = 0; // low
    // "sound-file" // filename TODO
    m_walletUpdateHints["desktop-entry"] = "org.flowee.pay";
    m_walletUpdateHints["urgency"] = 1; // normal

#if QT_DBUS_LIB
    // setup slots for DBUS-signals. Essentially remote callbacks from the deskop notifications app
    QDBusConnection::sessionBus().connect(QString(), QString(), "org.freedesktop.Notifications",
                                          "NotificationClosed", this, SLOT(notificationClosed(uint,uint)));
    QDBusConnection::sessionBus().connect(QString(), QString(), "org.freedesktop.Notifications",
                                          "ActionInvoked", this, SLOT(actionInvoked(uint,QString)));
#endif

    connect (this, SIGNAL(newBlockSeenSignal(int)), this, SLOT(newBlockSeen(int)), Qt::QueuedConnection);
    connect (this, SIGNAL(segmentUpdatedSignal()), this, SLOT(walletUpdated()), Qt::QueuedConnection);

    // debug
    QTimer::singleShot(3000, this, SLOT(test()));
    QTimer::singleShot(5000, this, SLOT(test()));
}

void NotificationManager::notifyNewBlock(const P2PNet::Notification &notification)
{
    emit newBlockSeenSignal(notification.blockHeight); // move to the Qt thread
}

void NotificationManager::segmentUpdated(const P2PNet::Notification&)
{
    emit segmentUpdatedSignal(); // move to the Qt thread
}

void NotificationManager::newBlockSeen(int blockHeight)
{
    if (m_newBlockMuted)
        return;
    logCritical() << "new block" << blockHeight;
#if QT_DBUS_LIB
    auto iface = remote();
    if (!iface->isValid()) return;
    QVariantList args;
    args << QVariant("Flowee Pay"); // app-name
    args << QVariant(m_blockNotificationId); // replaces-id
    args << QString(); // app_icon (not needed since we say which desktop file we are)
    args << QString(); // body-text
    args << tr("BCH block mined %1").arg(blockHeight); // summary text
    // args << QString(); // body-text
    QStringList actions; // actions
    actions << "mute" << tr("Mute");
    args << actions;
    args << m_newBlockHints;
    args << -1; // timeout (ms) -1 means to let the server decide
    if (!iface->callWithCallback("Notify", args, this,
                                 SLOT(newBlockNotificationShown(uint)))) {
        logWarning() << "dbus down, can't show notifications";
    }
#endif
}

void NotificationManager::newBlockNotificationShown(uint id)
{
    m_blockNotificationId = id;
    logDebug() << "new block notification id:" << m_blockNotificationId;
}

void NotificationManager::notificationClosed(uint32_t id, uint32_t reason)
{
    logDebug() << " something got closed" << id << reason;
    if (m_blockNotificationId == id) {
        m_blockNotificationId = 0;
    }
    else if (m_newFundsNotificationId == id) {
        m_newFundsNotificationId = 0;
        flushCollate();
    }
}

void NotificationManager::actionInvoked(uint, const QString &actionKey)
{
    if (actionKey == "mute")
        m_newBlockMuted = true;
}

void NotificationManager::walletUpdated()
{
    const auto data = collatedData();
    if (data.empty())
        return;

    int64_t deposited = 0;
    int64_t spent = 0;
    int txCount = 0;
    for (const auto &item : data) {
        deposited += item.deposited;
        spent += item.spent;
        txCount += item.txCount;
    }

#if QT_DBUS_LIB
    auto iface = remote();
    if (!iface->isValid()) return;
    QVariantList args;
    args << QVariant("Flowee Pay"); // app-name
    args << QVariant(m_newFundsNotificationId); // replaces-id
    args << QString(); // app_icon (not needed since we say which desktop file we are)
    args << tr("New Transaction", "", txCount);

    const auto gained = deposited - spent;
    auto pricesOracle = FloweePay::instance()->prices();
    QString gainedStr;
    if (pricesOracle->price() == 0)  {
        // no price data available (yet). Display crypto units
        gainedStr = QString("%1 %2")
                .arg(FloweePay::instance()->priceToStringPretty((double) gained),
                     FloweePay::instance()->unitName());

    } else {
        gainedStr = pricesOracle->formattedPrice(gained, pricesOracle->price());
    }
    if (gained > 0)
        gainedStr = QString("+%1").arg(gainedStr); // since we indicate adding, we always want the plus there

    // body-text
    if (data.size() > 1) {
        args << tr("%1 new transactions across %2 wallets found (%3)")
                .arg(txCount).arg(data.size())
                .arg(gainedStr);
    } else {
        args << tr("%1 new transactions found (%2)", "", txCount)
                .arg(txCount)
                .arg(gainedStr);
    }

    args << QStringList();
    args << m_walletUpdateHints;
    args << -1; // timeout (ms) -1 means to let the server decide
    if (!iface->callWithCallback("Notify", args, this,
                                 SLOT(walletUpdateNotificationShown(uint)))) {
        logWarning() << "dbus down, can't show notifications";
    }
#endif
}

void NotificationManager::walletUpdateNotificationShown(uint id)
{
    m_newFundsNotificationId = id;
}

void NotificationManager::test()
{
    emit newBlockSeenSignal(m_blockNotificationId == 0 ? 90 : 100);
    P2PNet::Notification data;
    data.blockHeight = 100;
    data.deposited = 10000000;
    data.spent = 750000;
    data.privacySegment = 9;
    data.txCount = 1;
    emit segmentUpdatedSignal();
    updateSegment(data);
}

#if QT_DBUS_LIB
QDBusInterface *NotificationManager::remote()
{
    if (m_remote == nullptr) {
        m_remote = new QDBusInterface("org.freedesktop.Notifications", // service
                                      "/org/freedesktop/Notifications", // path
                                      "org.freedesktop.Notifications", // interface name (duplicate of service)
                                      QDBusConnection::sessionBus(), this);
        if (!m_remote->isValid())
            logWarning() << qPrintable(QDBusConnection::sessionBus().lastError().message());
    }

    return m_remote;
}
#endif
