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
#include "WalletHistoryModel.h"
#include "FloweePay.h"

#include <BlockHeader.h>

#include <QLocale>
#include <QDateTime>
#include <QTimer>
#include <QThread>

/*
 * Attempt to add a transaction to this group.
 * Retuns false if the txIndex is not meant for this group
 */
bool WalletHistoryModel::TransactionGroup::add(int txIndex, uint32_t timestamp, const QDate &today)
{
    if (startTxIndex == -1) {
        startTxIndex = txIndex;

        // first one in this group. Now we need to decide which period we area actually looking at.
        QDate date = QDateTime::fromSecsSinceEpoch(timestamp).date();
        int days = 0;
        if (date == today) {
            period = WalletEnums::Today;
            days = 1;
        }
        else if (date == today.addDays(-1)) {
            period = WalletEnums::Yesterday;
            days = 1;
        }
        else if (date >= today.addDays(-1 * today.dayOfWeek() + 1)) {
            // this week
            period = WalletEnums::EarlierThisWeek;
            date = date.addDays(-1 * date.dayOfWeek());
            const auto yesterday = today.addDays(-1);
            days = date.daysTo(yesterday);
        }
        else { // a whole month.
            period = WalletEnums::Month;
            if (date >= today.addDays(-1 * today.day() + 1)) // special case THIS month
                period = WalletEnums::EarlierThisMonth;

            days = date.daysInMonth();
            date = date.addDays(-1 * date.day() + 1);
            const auto endOfMonth = date.addDays(days);
            // make it smaller if needed.
            const auto startThisWeek = today.addDays(-1 * today.dayOfWeek() + 1);
            if (startThisWeek < endOfMonth) {
                days = startThisWeek.day() - 1;
            }
            else {
                // date = date.addDays(-1 * date.day() + 1);
                const auto yesterday = today.addDays(-1);
                if (yesterday < endOfMonth)  // don't eat the events that happend yesterday.
                    days -= 1;
            }
        }
        const QDateTime dt(date, QTime());
        endTime = dt.addDays(days).toSecsSinceEpoch() - 1;
    }
    else if (timestamp > endTime) {
        // doesn't fit in our time-period.
        return false;
    }
    endTxIndex = txIndex;
    return true;
}

// -------


WalletHistoryModel::WalletHistoryModel(Wallet *wallet, QObject *parent)
    : QAbstractListModel(parent),
      m_wallet(wallet)
{
    QMutexLocker locker(&m_wallet->m_lock);
    assert(wallet->segment());
    resetLastSyncIndicator();
    QTimer::singleShot(0, this, SLOT(createMap()));

    connect(wallet, SIGNAL(appendedTransactions(int,int)), SLOT(appendTransactions(int,int)), Qt::QueuedConnection);
    connect(wallet, SIGNAL(transactionConfirmed(int)), SLOT(transactionChanged(int)), Qt::QueuedConnection);
    connect(wallet, SIGNAL(transactionChanged(int)), SLOT(transactionChanged(int)), Qt::QueuedConnection);
    connect(wallet, SIGNAL(transactionRemoved(int)), SLOT(removeTransaction(int)), Qt::QueuedConnection);
}

int WalletHistoryModel::rowCount(const QModelIndex &parent) const
{
    assert(QThread::currentThread() == thread());
    if (parent.isValid()) // only for the (invalid) root node we return a count, since this is a list not a tree
        return 0;

    return m_rowsProxy.size();
}

QVariant WalletHistoryModel::data(const QModelIndex &index, int role) const
{
    assert(QThread::currentThread() == thread());
    if (!index.isValid())
        return QVariant();

    QMutexLocker locker(&m_wallet->m_lock);

    assert(m_rowsProxy.size() > index.row());
    assert(index.row() >= 0);
    const int txIndex = txIndexFromRow(index.row());
    // logDebug() << " getting" << index.row() << "=>" << txIndex;
    auto itemIter = m_wallet->m_walletTransactions.find(txIndex);
    if (itemIter == m_wallet->m_walletTransactions.end()) // the wallet changed, we're probably waiting for a queued connection
        return QVariant();
    const auto &item = itemIter->second;
    switch (role) {
    case TxId:
        return QVariant(QString::fromStdString(item.txid.ToString()));
    case MinedHeight:
        return QVariant(item.minedBlockHeight);
    case TxDate: {
        int64_t timestamp = item.transactionTime;
        if (timestamp == 0) {
            // lets see if it was mined, we then take the blocktime
            if (item.minedBlockHeight > 0)
                timestamp = secsSinceEpochFor(item.minedBlockHeight);
        }
        if (timestamp == 0)
            return QVariant(); // undefined.
        return QVariant(QDateTime::fromSecsSinceEpoch(timestamp));
    }
    case FundsIn: {
        qint64 value = 0;
        for (auto input : item.inputToWTX) {
            auto key = input.second;
            int outputIndex = key & 0xFFFF;
            assert(outputIndex >= 0);
            key >>= 16;

            assert(m_wallet->m_walletTransactions.find(key) != m_wallet->m_walletTransactions.end());
            const auto &spendingTx = m_wallet->m_walletTransactions.at(key);
            assert(spendingTx.outputs.find(outputIndex) != spendingTx.outputs.end());
            auto prevOut = spendingTx.outputs.at(outputIndex);
            value += prevOut.value;
        }
        return QVariant(double(value));
    }
    case FundsOut: {
        qint64 value = 0;
        for (auto out : item.outputs) {
            value += out.second.value;
        }
        return QVariant(double(value));
    }
    case WalletIndex:
        return QVariant(txIndex);
    case NewTransaction:
        return QVariant(item.minedBlockHeight > m_lastSyncIndicator || item.isUnconfirmed());
    case IsCoinbase:
        return QVariant(item.isCoinbase);
    case IsFused:
        return QVariant(item.isCashFusionTx);
    case Comment:
        return QVariant(item.userComment);
    case PlacementInGroup: {
        // notice that since we invert the ordering, the end/start get inverted too
        const auto &group = m_groups.at(groupIdForTxIndex(txIndex));
        const bool start = group.startTxIndex == txIndex;
        const bool end = group.endTxIndex == txIndex;
        if (start && end)
            return WalletEnums::Ungrouped;
        if (start)
            return WalletEnums::GroupEnd;
        if (end)
            return WalletEnums::GroupStart;
        return WalletEnums::GroupMiddle;
    }
    case GroupId:
        return groupIdForTxIndex(txIndex);
    }

    return QVariant();
}

int WalletHistoryModel::groupIdForTxIndex(int txIndex) const
{
    assert(QThread::currentThread() == thread());
    // Method is const because the cache is mutable.
    if (m_groupCache.txIndex != txIndex) {
        for (size_t i = 0; i < m_groups.size(); ++i) {
            if (txIndex >= m_groups.at(i).startTxIndex && txIndex <= m_groups.at(i).endTxIndex) {
                m_groupCache = { txIndex, (int) i };
                break;
            }
        }
    }
    assert(m_groupCache.txIndex == txIndex);
    return m_groupCache.groupId;
}


QString WalletHistoryModel::groupingPeriod(int groupId) const
{
    assert(QThread::currentThread() == thread());
    if (groupId < 0 || groupId >= static_cast<int>(m_groups.size()))
        return "bad groupId";
    switch (m_groups.at(groupId).period) {
    case WalletEnums::Today:
        return tr("Today");
    case WalletEnums::Yesterday:
        return tr("Yesterday");
    case WalletEnums::EarlierThisWeek:
        return tr("Earlier this week");
    case WalletEnums::Week:
        return tr("This week");
    case WalletEnums::EarlierThisMonth:
        return tr("Earlier this month");
    case WalletEnums::Month:
    default: {
        static const QString wide("MMMM yyyy");
        static const QString lean("MMMM");
        const uint32_t timestamp = m_groups.at(groupId).endTime;
        const QDate date = QDateTime::fromSecsSinceEpoch(timestamp).date();
        return QLocale::system().toString(date,
                                          date.year() == m_today.year() ? lean : wide);
    }
    }
}

QHash<int, QByteArray> WalletHistoryModel::roleNames() const
{
    assert(QThread::currentThread() == thread());
    QHash<int, QByteArray> answer;
    answer[NewTransaction] = "isNew"; // Deprecated
    answer[TxId] = "txid";
    answer[MinedHeight] = "height";
    answer[TxDate] = "date";
    answer[FundsIn] = "fundsIn";
    answer[FundsOut] = "fundsOut";
    answer[WalletIndex] = "walletIndex";
    answer[IsCoinbase] = "isCoinbase";
    answer[IsFused] = "isFused";
    answer[Comment] = "comment";
    answer[PlacementInGroup] = "placementInGroup";
    answer[GroupId] = "grouping";

    return answer;
}

QString WalletHistoryModel::dateForItem(qreal offset) const
{
    assert(QThread::currentThread() == thread());
    if (m_rowsProxy.isEmpty())
        return QString();
    if (std::isnan(offset) || offset < 0 || offset > 1.0)
        return QString();
    const int row = std::round(offset * m_rowsProxy.size());
    if (row >= m_rowsProxy.size())
        return QString();
    QMutexLocker locker(&m_wallet->m_lock);
    auto txIter = m_wallet->m_walletTransactions.find(txIndexFromRow(row));
    if (txIter == m_wallet->m_walletTransactions.end()) {
        // it is possible for a transaction to be removed async...
        return QString();
    }
    const auto &item = txIter->second;
    if (item.minedBlockHeight <= 0)
        return QString();
    auto timestamp = secsSinceEpochFor(item.minedBlockHeight);
    if (timestamp == 0)
        return QString();
    auto dt = QDateTime::fromSecsSinceEpoch(timestamp);
    static const QString format("MMMM yyyy");
    return QLocale::system().toString(dt, format);
}

void WalletHistoryModel::appendTransactions(int firstNew, int count)
{
    assert(QThread::currentThread() == thread());
    QMutexLocker locker(&m_wallet->m_lock);
    const auto oldCount = m_rowsProxy.size();
    for (auto i = firstNew; i < firstNew + count; ++i) {
        auto iter = m_wallet->m_walletTransactions.find(i);
        if (iter == m_wallet->m_walletTransactions.end())
            continue; // already removed by wallet again.
        if (!filterTransaction(iter->second))
            continue;
        m_rowsProxy.push_back(i);
        addTxIndexToGroups(iter->first, iter->second.minedBlockHeight);
    }
    auto insertedCount = m_rowsProxy.size() - oldCount;
    if (m_rowsSilentlyInserted == -1 && insertedCount) {
        if (oldCount) {
            // Due to grouping being drawn in relation, we need to force
            // a change in the previously top item
            // remove the top one
            beginRemoveRows(QModelIndex(), 0, 0);
            endRemoveRows();
            ++insertedCount;
        }
        // and insert the new ones, plus the one we just removed.
        beginInsertRows(QModelIndex(), 0, insertedCount - 1);
        endInsertRows();
    }
    else {
        m_rowsSilentlyInserted += insertedCount;
    }
}

void WalletHistoryModel::removeTransaction(int txIndex)
{
    assert(QThread::currentThread() == thread());
    const int index = m_rowsProxy.indexOf(txIndex);
    if (index == -1) // probably never got inserted
        return;
    int row = m_rowsProxy.size() - index - 1;
    m_rowsProxy.removeAt(index);
    if (m_rowsSilentlyInserted > 0)
        row -= m_rowsSilentlyInserted;
    beginRemoveRows(QModelIndex(), row, row);
    endRemoveRows();
}

void WalletHistoryModel::transactionChanged(int txIndex)
{
    assert(QThread::currentThread() == thread());
    const int index = m_rowsProxy.indexOf(txIndex);
    if (index == -1) // probably never got inserted
        return;
    int row = m_rowsProxy.size() - index - 1;
    if (m_rowsSilentlyInserted > 0)
        row -= m_rowsSilentlyInserted;
    // update row, the 'minedHeight' went from unset to an actual value
    beginRemoveRows(QModelIndex(), row, row);
    endRemoveRows();
    beginInsertRows(QModelIndex(), row, row);
    endInsertRows();
}

void WalletHistoryModel::createMap()
{
    assert(QThread::currentThread() == thread());
    m_recreateTriggered = false;
    m_groups.clear();
    m_today = today();
    m_groupCache = { -1 , 0 };
    if (!m_rowsProxy.isEmpty()) {
        beginRemoveRows(QModelIndex(), 0, m_rowsProxy.size() - 1);
        m_rowsProxy.clear();
        endRemoveRows();
    }

    // we insert the key used in the m_wallet->m_walletTransaction map
    // in the order of how our rows work here.
    // This is oldest to newest, which is how our model is also structured.
    {
        QMutexLocker locker(&m_wallet->m_lock);
        m_rowsProxy.reserve(m_wallet->m_walletTransactions.size());
        for (const auto &iter : m_wallet->m_walletTransactions) {
            if (!filterTransaction(iter.second))
                continue;
            m_rowsProxy.push_back(iter.first);
            // Last, resolve grouping
            addTxIndexToGroups(iter.first, iter.second.minedBlockHeight);
        }
    }

    if (!m_rowsProxy.isEmpty()) {
        beginInsertRows(QModelIndex(), 0, m_rowsProxy.size() - 1);
        endInsertRows();
    }
}

bool WalletHistoryModel::filterTransaction(const Wallet::WalletTransaction &wtx) const
{
    assert(QThread::currentThread() == thread());
    if (!m_includeFlags.testFlag(WalletEnums::IncludeUnconfirmed) && wtx.isUnconfirmed())
        return false;
    if (!m_includeFlags.testFlag(WalletEnums::IncludeRejected) && wtx.isRejected())
        return false;
    if (!m_includeFlags.testFlag(WalletEnums::IncludeConfirmed) && !wtx.isUnconfirmed())
        return false;
    return true;
}

void WalletHistoryModel::addTxIndexToGroups(int txIndex, int blockheight)
{
    assert(QThread::currentThread() == thread());
    if (m_groups.empty())
        m_groups.push_back(TransactionGroup());

    QMutexLocker locker(&m_wallet->m_lock);
    uint32_t timestamp = 0;
    auto txIter = m_wallet->m_walletTransactions.find(txIndex);
    if (txIter != m_wallet->m_walletTransactions.end())
        timestamp = txIter->second.transactionTime;

    if (timestamp == 0 && blockheight > 0) // We only know when it was mined
        timestamp = secsSinceEpochFor(blockheight);

    if (timestamp == 0) {
        // we should really not get into this if(), but only in 2024.01 did we save the
        // time of the transaction, so older ones are to be treated special.
        if (blockheight == Wallet::Rejected) {
            // we don't actually know when a rejected transaction was made which didn't
            // save it yet, back then. We can only guesstimeate and show that.

            const auto nope = m_wallet->m_walletTransactions.end();
            auto txIter2 = txIter;
            while (timestamp == 0 && txIter2 != nope) {
                if (txIter2->second.minedBlockHeight > 0) {
                    timestamp = secsSinceEpochFor(txIter2->second.minedBlockHeight);
                    break;
                }
                txIter2++;
            }
            // other direction.
            txIter2 = txIter;
            while (timestamp == 0 && txIter2 != m_wallet->m_walletTransactions.begin()) {
                if (txIter2->second.minedBlockHeight > 0) {
                    timestamp = secsSinceEpochFor(txIter2->second.minedBlockHeight);
                    break;
                }
                txIter2--;
            }

            // update the wallet to avoid this whole calc next time around.
            txIter->second.transactionTime = timestamp;
        }
        if (timestamp == 0) // probably an unconfirmed transaction.
            timestamp = time(nullptr);
    }

    if (!m_groups.back().add(txIndex, timestamp, m_today)) {
        // didn't fit, make a new group and add it there.
        TransactionGroup newGroup;
        newGroup.period = m_groups.back().period;
        bool ok = newGroup.add(txIndex, timestamp, m_today);
        assert (ok);
        m_groups.push_back(newGroup);
    }
}

int WalletHistoryModel::txIndexFromRow(int row) const
{
    assert(QThread::currentThread() == thread());
    int newRow = m_rowsProxy.size() - row - 1;
    if (m_rowsSilentlyInserted > 0)
        newRow -= m_rowsSilentlyInserted;
    assert(newRow >= 0);
    assert(newRow < m_rowsProxy.size());
    return m_rowsProxy.at(newRow);
}

const QFlags<WalletEnums::Include> &WalletHistoryModel::includeFlags() const
{
    assert(QThread::currentThread() == thread());
    return m_includeFlags;
}

void WalletHistoryModel::setIncludeFlags(const QFlags<WalletEnums::Include> &flags)
{
    assert(QThread::currentThread() == thread());
    if (m_includeFlags == flags)
        return;
    m_includeFlags = flags;
    emit includeFlagsChanged();

    if (m_recreateTriggered)
        return;
    m_recreateTriggered = true;
    QTimer::singleShot(0, this, SLOT(createMap()));
}

void WalletHistoryModel::freezeModel(bool on)
{
    assert(QThread::currentThread() == thread());
    if ((on && m_rowsSilentlyInserted >= 0) || (!on && m_rowsSilentlyInserted < 0))
        return;
    if (on) {
        m_rowsSilentlyInserted = 0;
    }
    else {
        // process backlog before turning off
        if (m_rowsSilentlyInserted > 0) {
            beginRemoveRows(QModelIndex(), 0, 0);
            endRemoveRows();
            beginInsertRows(QModelIndex(), 0, m_rowsSilentlyInserted);
            endInsertRows();
        }
        m_rowsSilentlyInserted = -1;
    }
    emit freezeModelChanged();
}

bool WalletHistoryModel::isModelFrozen() const
{
    assert(QThread::currentThread() == thread());
    return m_rowsSilentlyInserted >= 0;
}

uint32_t WalletHistoryModel::secsSinceEpochFor(int blockHeight) const
{
    assert(QThread::currentThread() == thread());
    assert(blockHeight > 0);
    // wrap this for convenience and also ensure that we never return an insanely old
    // date (1970) just because we lack blockheader data.
    try {
        return std::max<uint32_t>(1250000000,
                FloweePay::instance()->p2pNet()->blockchain().block(blockHeight).nTime);
    } catch (const std::exception &e) {
        // a blockheight we don't have.
        assert(false); // for debug builds, please figure out if the caller can be improved
        return 0;
    }
}

QDate WalletHistoryModel::today() const
{
    assert(QThread::currentThread() == thread());
    return QDate::currentDate();
}

int WalletHistoryModel::lastSyncIndicator() const
{
    assert(QThread::currentThread() == thread());
    return m_lastSyncIndicator;
}

void WalletHistoryModel::setLastSyncIndicator(int)
{
    // only reset is allowed.
    // From QML:   `lastSyncIndicator = undefined;`
    assert(false);
}

void WalletHistoryModel::resetLastSyncIndicator()
{
    assert(QThread::currentThread() == thread());
    QMutexLocker locker(&m_wallet->m_lock);
    const auto old = m_lastSyncIndicator;
    m_lastSyncIndicator = m_wallet->segment()->lastBlockSynched();

    int index = m_rowsProxy.size() - 1; // newest is at the end
    while (index >= 0) {
        const auto &tx = m_wallet->m_walletTransactions.find(m_rowsProxy.at(index));
        if (tx->second.minedBlockHeight > 0 && tx->second.minedBlockHeight < old)
            break;
        --index;
    }
    const int lastRow = m_rowsProxy.size() - std::max(0, index) - 1;
    // refresh the rows that need the 'new' indicator removed.
    if (lastRow > 0) {
        assert(lastRow < m_rowsProxy.size());
        beginRemoveRows(QModelIndex(), 0, lastRow);
        endRemoveRows();
        beginInsertRows(QModelIndex(), 0, lastRow);
        endInsertRows();
    }
}
