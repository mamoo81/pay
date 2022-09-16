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

#if QT_DBUS_LIB
#include <QDBusConnection>
#include <QDBusInterface>
#endif
#include <QSettings>

constexpr const char *MUTE = "notificationNewblockMute";

NotificationManager::NotificationManager(QObject *parent)
    : QObject(parent),
      m_startupTime(QDateTime::currentDateTimeUtc())
{
    setCollation(true);

#if QT_DBUS_LIB
    // We use the notification spec (v 1.2)
    // https://specifications.freedesktop.org/notification-spec/notification-spec-latest.html
    m_newBlockHints["desktop-entry"] = "org.flowee.pay";
    m_newBlockHints["urgency"] = 0; // low
    // "sound-file" // filename TODO
    m_walletUpdateHints["desktop-entry"] = "org.flowee.pay";
    m_walletUpdateHints["urgency"] = 1; // normal

    // setup slots for DBUS-signals. Essentially remote callbacks from the deskop notifications app
    QDBusConnection::sessionBus().connect(QString(), QString(), "org.freedesktop.Notifications",
                                          "NotificationClosed", this, SLOT(notificationClosed(uint,uint)));
    QDBusConnection::sessionBus().connect(QString(), QString(), "org.freedesktop.Notifications",
                                          "ActionInvoked", this, SLOT(actionInvoked(uint,QString)));
#endif

    connect (this, SIGNAL(newBlockSeenSignal(int)), this, SLOT(newBlockSeen(int)), Qt::QueuedConnection);
    connect (this, SIGNAL(segmentUpdatedSignal()), this, SLOT(walletUpdated()), Qt::QueuedConnection);

    QSettings appConfig;
    m_newBlockMuted = appConfig.value(MUTE, false).toBool();
}

void NotificationManager::notifyNewBlock(const P2PNet::Notification &notification)
{
    emit newBlockSeenSignal(notification.blockHeight); // move to the Qt thread
}

void NotificationManager::segmentUpdated(const P2PNet::Notification&)
{
    emit segmentUpdatedSignal(); // move to the Qt thread
}

bool NotificationManager::newBlockMuted() const
{
    return m_newBlockMuted;
}

void NotificationManager::setNewBlockMuted(bool mute)
{
    if (m_newBlockMuted == mute)
        return;
    m_newBlockMuted = mute;
    QSettings appConfig;
    appConfig.setValue(MUTE, m_newBlockMuted);
    emit newBlockMutedChanged();
}

void NotificationManager::newBlockSeen(int blockHeight)
{
    if (m_newBlockMuted)
        return;
    if (m_startupTime.secsTo(QDateTime::currentDateTimeUtc()) < 60) {
        // When Flowee Pay first starts it synchronizes with the network
        // and we get a 'new block seen' notification almost every time.
        // This is annoying and useless, so lets ignore the new blocks
        // happening the first minute or so. Which is almost always
        // enough time to sync the headers.
        return;
    }
    logCritical() << "new block" << blockHeight;
#if QT_DBUS_LIB
    auto iface = remote();
    if (!iface->isValid()) return;
    QVariantList args;
    args << QVariant("Flowee Pay"); // app-name
    args << QVariant(m_blockNotificationId); // replaces-id
    args << QString(); // app_icon (not needed since we say which desktop file we are)
    args << QString(); // body-text
    if (FloweePay::instance()->chain() == P2PNet::MainChain)
        args << tr("Bitcoin Cash block mined. Height: %1").arg(blockHeight); // summary text
    else
        args << tr("tBCH (testnet4) block mined: %1").arg(blockHeight); // summary text
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
#if QT_DBUS_LIB
    m_blockNotificationId = id;
    logDebug() << "new block notification id:" << m_blockNotificationId;
#endif
}

void NotificationManager::notificationClosed(uint32_t id, uint32_t reason)
{
#if QT_DBUS_LIB
    logDebug() << " something got closed" << id << reason;
    if (m_blockNotificationId == id) {
        m_blockNotificationId = 0;
    }
    else if (m_newFundsNotificationId == id) {
        m_newFundsNotificationId = 0;
        flushCollate();
    }
#endif
}

void NotificationManager::actionInvoked(uint, const QString &actionKey)
{
    if (actionKey == "mute")
        setNewBlockMuted(true);
}

void NotificationManager::walletUpdated()
{
    const auto data = collatedData();
    if (data.empty())
        return;

    if (m_openingNewFundsNotification) {
        // we are currently (async) opening a notification, wait until its actually open.
       QTimer::singleShot(50, this, SLOT(walletUpdated()));
       return;
    }

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
    if (pricesOracle->price())
        gainedStr = pricesOracle->formattedPrice(gained, pricesOracle->price());
    if (gainedStr.isEmpty())  {
        // no price data available (yet). Display crypto units
        gainedStr = QString("%1 %2")
                .arg(FloweePay::instance()->amountToStringPretty((double) gained),
                     FloweePay::instance()->unitName());
    }
    if (gained > 0) // since we indicate adding, we always want the plus there
        gainedStr = QString("+%1").arg(gainedStr);

    // body-text
    if (data.size() > 1) {
        args << tr("%1 new transactions across %2 wallets found (%3)")
                .arg(txCount).arg(data.size())
                .arg(gainedStr);
    }
    else if (gained < 0 && txCount == 1) {
        args << tr("A payment of %1 has been sent")
                .arg(gainedStr.mid(1));
    }
    else {
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
    if (m_newFundsNotificationId == 0) {
        // The assignment of m_newFundsNotificationId is async, as such
        // we want to remember we are opening one and not have multiple calls in
        // flight at the same time
        m_openingNewFundsNotification = true;
    }
#endif
}

void NotificationManager::walletUpdateNotificationShown(uint id)
{
#if QT_DBUS_LIB
    m_newFundsNotificationId = id;
    m_openingNewFundsNotification = false;
#endif
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
