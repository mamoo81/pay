/*
 * This file is part of the Flowee project
 * Copyright (C) 2020-2022 Tom Zander <tom@flowee.org>
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

/*
 * Attempt to add a transaction to this group.
 * Retuns false if the txIndex is not meant for this group
 */
bool WalletHistoryModel::TransactionGroup::add(int txIndex, uint32_t timestamp)
{
    if (startTxIndex == -1) {
        startTxIndex = txIndex;

        // first one in this group. Now we need to decide which period we area actually looking at.
        QDate date = QDateTime::fromSecsSinceEpoch(timestamp).date();
        int days = 0;
        QDate today = QDate::currentDate();
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
            days = yesterday.day() - date.day();
        }
        else if (date >= today.addDays(-1 * today.day() + 1)) {
            // this month
            period = WalletEnums::EarlierThisMonth;
            date = date.addDays(-1 * date.day() + 1);
            const auto weekStart = today.addDays(-1 * today.dayOfWeek() + 1);
            days = weekStart.day() - 1;

            const auto yesterday = today.addDays(-1);
            if (yesterday.year() == date.year() && yesterday.month() == date.month()) {
                // don't eat the events that happend yesterday.
                days -= 1;
            }
        }
        else { // any (other) month
            period = WalletEnums::Month;
            date = date.addDays(-1 * date.day() + 1);
            days = date.daysInMonth();

            const auto yesterday = today.addDays(-1);
            if (yesterday.year() == date.year() && yesterday.month() == date.month()) {
                // don't eat the events that happend yesterday.
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
    createMap();

    connect(wallet, SIGNAL(appendedTransactions(int,int)), SLOT(appendTransactions(int,int)), Qt::QueuedConnection);
    connect(wallet, SIGNAL(transactionConfirmed(int)), SLOT(transactionChanged(int)), Qt::QueuedConnection);
    connect(wallet, SIGNAL(transactionChanged(int)), SLOT(transactionChanged(int)), Qt::QueuedConnection);
}

int WalletHistoryModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) // only for the (invalid) root node we return a count, since this is a list not a tree
        return 0;

    return m_rowsProxy.size();
}

QVariant WalletHistoryModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    QMutexLocker locker(&m_wallet->m_lock);

    assert(m_rowsProxy.size() > index.row());
    assert(index.row() >= 0);
    const int txIndex = m_rowsProxy.at(m_rowsProxy.size() - index.row() - 1); // reverse index to make the VIEW have newest at the top
    // logDebug() << " getting" << index.row() << "=>" << txIndex;
    auto itemIter = m_wallet->m_walletTransactions.find(txIndex);
    assert(itemIter != m_wallet->m_walletTransactions.end());
    if (itemIter == m_wallet->m_walletTransactions.end())
        return QVariant();
    const auto &item = itemIter->second;
    switch (role) {
    case TxId:
        return QVariant(QString::fromStdString(item.txid.ToString()));
    case MinedHeight:
        return QVariant(item.minedBlockHeight);
    case MinedDate: {
        if (item.minedBlockHeight <= 0)
            return QVariant();

        auto timestamp = secsSinceEpochFor(item.minedBlockHeight);
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
    case IsCashFusion:
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
        uint32_t timestamp = m_groups.at(groupId).endTime;
        QDate date = QDateTime::fromSecsSinceEpoch(timestamp).date();
        if (date.year() == QDate::currentDate().year())
            return date.toString("MMMM");
        return date.toString("MMMM yyyy");
    }
    }
}

QHash<int, QByteArray> WalletHistoryModel::roleNames() const
{
    QHash<int, QByteArray> answer;
    answer[NewTransaction] = "isNew";
    answer[TxId] = "txid";
    answer[MinedHeight] = "height";
    answer[MinedDate] = "date";
    answer[FundsIn] = "fundsIn";
    answer[FundsOut] = "fundsOut";
    answer[WalletIndex] = "walletIndex";
    answer[IsCoinbase] = "isCoinbase";
    answer[IsCashFusion] = "isCashFusion";
    answer[Comment] = "comment";
    answer[PlacementInGroup] = "placementInGroup";
    answer[GroupId] = "grouping";
    // answer[SavedFiatRate] = "savedFiatRate";

    return answer;
}

QString WalletHistoryModel::dateForItem(qreal offset) const
{
    if (m_rowsProxy.isEmpty())
        return QString();
    if (std::isnan(offset) || offset < 0 || offset > 1.0)
        return QString();
    const size_t row = std::round(offset * m_rowsProxy.size());
    auto item = m_wallet->m_walletTransactions.at(
                m_rowsProxy.at(m_rowsProxy.size() - row - 1));
    if (item.minedBlockHeight <= 0)
        return QString();
    auto timestamp = secsSinceEpochFor(item.minedBlockHeight);
    if (timestamp == 0)
        return QString();
    return QDateTime::fromSecsSinceEpoch(timestamp).toString("MMMM yyyy");
}

void WalletHistoryModel::appendTransactions(int firstNew, int count)
{
    const auto oldCount = m_rowsProxy.size();
    for (auto i = firstNew; i < firstNew + count; ++i) {
        auto iter = m_wallet->m_walletTransactions.find(i);
        if (!filterTransaction(iter->second))
            continue;
        m_rowsProxy.push_back(i);
        addTxIndexToGroups(iter->first, iter->second.minedBlockHeight);
    }
    auto insertedCount = m_rowsProxy.size() - oldCount;
    if (insertedCount) {
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
}

void WalletHistoryModel::transactionChanged(int txIndex)
{
    const int row = m_rowsProxy.size() - m_rowsProxy.indexOf(txIndex) - 1;
    // update row, the 'minedHeight' went from unset to an actual value
    beginRemoveRows(QModelIndex(), row, row);
    endRemoveRows();
    beginInsertRows(QModelIndex(), row, row);
    endInsertRows();
}

void WalletHistoryModel::createMap()
{
    m_recreateTriggered = false;
    m_rowsProxy.clear();
    m_rowsProxy.reserve(m_wallet->m_walletTransactions.size());
    m_groups.clear();

    // we insert the key used in the m_wallet->m_walletTransaction map
    // in the order of how our rows work here.
    // This is oldest to newest, which is how our model is also structured.
    for (const auto &iter : m_wallet->m_walletTransactions) {
        if (!filterTransaction(iter.second))
            continue;
        m_rowsProxy.push_back(iter.first);
        // Last, resolve grouping
        addTxIndexToGroups(iter.first, iter.second.minedBlockHeight);
    }

    m_groupCache = { -1 , 0 };
}

bool WalletHistoryModel::filterTransaction(const Wallet::WalletTransaction &wtx) const
{
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
    if (m_groups.empty())
        m_groups.push_back(TransactionGroup());

    uint32_t timestamp;
    if (blockheight <= 0) {
        timestamp = time(nullptr);
    } else {
        timestamp = secsSinceEpochFor(blockheight);
    }
    assert(timestamp > 0);

    if (!m_groups.back().add(txIndex, timestamp)) {
        // didn't fit, make a new group and add it there.
        TransactionGroup newGroup;
        newGroup.period = m_groups.back().period;
        bool ok = newGroup.add(txIndex, timestamp);
        assert (ok);
        m_groups.push_back(newGroup);
    }
}

const QFlags<WalletEnums::Include> &WalletHistoryModel::includeFlags() const
{
    return m_includeFlags;
}

void WalletHistoryModel::setIncludeFlags(const QFlags<WalletEnums::Include> &flags)
{
    if (m_includeFlags == flags)
        return;
    m_includeFlags = flags;
    emit includeFlagsChanged();

    if (m_recreateTriggered)
        return;
    m_recreateTriggered = true;
    QTimer::singleShot(0, this, SLOT(createMap()));
}

uint32_t WalletHistoryModel::secsSinceEpochFor(int blockHeight) const
{
    return FloweePay::instance()->p2pNet()->blockchain().block(blockHeight).nTime;
}

int WalletHistoryModel::lastSyncIndicator() const
{
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
    const auto old = m_lastSyncIndicator;
    m_lastSyncIndicator = m_wallet->segment()->lastBlockSynched();

    int index = m_rowsProxy.size() - 1; // newest is at the end
    while (index >= 0) {
        const auto &tx = m_wallet->m_walletTransactions.find(m_rowsProxy.at(index));
        if (tx->second.minedBlockHeight > 0 && tx->second.minedBlockHeight < old)
            break;
        --index;
    }
    const int lastRow = m_rowsProxy.size() - index - 1;
    // refresh the rows that need the 'new' indicator removed.
    if (lastRow > 0) {
        beginRemoveRows(QModelIndex(), 0, lastRow);
        endRemoveRows();
        beginInsertRows(QModelIndex(), 0, lastRow);
        endInsertRows();
    }
}
