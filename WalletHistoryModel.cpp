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
#include "Wallet.h"
#include "FloweePay.h"

#include <BlockHeader.h>

#include <QLocale>
#include <QDateTime>
#include <QTimer>

namespace {

struct TransactionGroup;
class TransactionGroupingResolver
{
public:
    std::vector<TransactionGroup> m_groups;

    void add(int index, int blockheight);
};

struct TransactionGroup
{
    enum GroupingPeriod {
        Today,
        Yesterday,
        EarlierThisWeek, // this week, but we grouped some in the previous category(s)
        Week,
        EarlierThisMonth, // this month, but we grouped some in the previous category(s)
        Month,
        Unset
    };

    GroupingPeriod period = Unset;
    int startTxIndex = -1;
    int endTxIndex = -1;
    uint32_t startTime = 0;
    uint32_t endTime = 0;

    bool add(int index, uint32_t timestamp);
};

void TransactionGroupingResolver::add(int index, int blockheight)
{
    if (m_groups.empty())
        m_groups.push_back(TransactionGroup());

    if (blockheight <= 0) {
        assert(m_groups.size() == 1);
        // not (yet) confirmed.
        m_groups.back().add(index, 0);
        return;
    }

    const auto &bc = FloweePay::instance()->p2pNet()->blockchain();
    uint32_t timestamp = bc.block(blockheight).nTime;
    assert(timestamp > 0);

    if (!m_groups.back().add(index, timestamp)) {
        TransactionGroup newGroup;
        newGroup.period = m_groups.back().period;
        bool ok = newGroup.add(index, timestamp);
        assert (ok);
        m_groups.push_back(newGroup);
    }
}

/*
 * Attempt to add a transaction to this group.
 */
bool TransactionGroup::add(int index, uint32_t timestamp)
{
    if (startTxIndex == -1) {
        endTxIndex = index;

        // first one in this group. Now we need to decide which period we area actually looking at.
        QDate date = QDateTime::fromSecsSinceEpoch(timestamp).date();
        int days = 0;

        if (period != Month) {
            QDate today = QDate::currentDate();
            if (date == today) {
                period = Today;
                days = 1;
            }
            else if (date == today.addDays(-1)) {
                period = Yesterday;
                date = date.addDays(-1);
                days = 1;
            }
            else if (date > today.addDays(-1 * today.dayOfWeek() + 1)) {
                // this week
                period = period == Unset ? Week : EarlierThisWeek;
                date = date.addDays(-1 * date.dayOfWeek());
                days = 7;
            }
            else if (date > today.addDays(-1 * today.day() + 1)) {
                // this month
                period = EarlierThisMonth;
                date = date.addDays(-1 * date.day() + 1);
                days = date.daysInMonth();
            }
        }
        if (days == 0) { // any (other) month
            period = Month;
            date = date.addDays(-1 * date.day() + 1);
            days = date.daysInMonth();
        }
        assert(days > 0);
        const QDateTime dt(date, QTime());
        startTime = dt.toSecsSinceEpoch();
        endTime = dt.addDays(days).toSecsSinceEpoch() - 1;
    }
    else if (timestamp < startTime) {
        // doesn't fit in our time-period.
        return false;
    }
    startTxIndex = index;
    return true;
}

};


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
    const int txIndex = m_rowsProxy.at(index.row());
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
        auto header = FloweePay::instance()->p2pNet()->blockchain().block(item.minedBlockHeight);
        return QVariant(QDateTime::fromSecsSinceEpoch(header.nTime));
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
        return QVariant(m_rowsProxy.at(index.row()));
    case NewTransaction:
        return QVariant(item.minedBlockHeight > m_lastSyncIndicator || item.isUnconfirmed());
    case IsCoinbase:
        return QVariant(item.isCoinbase);
    case IsCashFusion:
        return QVariant(item.isCashFusionTx);
    case Comment:
        return QVariant(item.userComment);
    case ItemGroupInfo: {
        const auto &group = m_groups.at(groupIdForTxIndex(txIndex));
        const bool start = group.startTxIndex == txIndex;
        const bool end = group.endTxIndex == txIndex;
        if (start && end)
            return WalletEnums::Ungrouped;
        // notice that since we invert the ordering, the end/start get inverted too
        if (start)
            return WalletEnums::GroupEnd;
        if (end)
            return WalletEnums::GroupStart;
        return WalletEnums::GroupMiddle;
    }
    case ItemGroupPeriod:
        switch (m_groups.at(groupIdForTxIndex(txIndex)).period) {
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
            const auto &bc = FloweePay::instance()->p2pNet()->blockchain();
            uint32_t timestamp = bc.block(itemIter->second.minedBlockHeight).nTime;
            assert(timestamp > 0);
            QDate date = QDateTime::fromSecsSinceEpoch(timestamp).date();
            return date.toString("MMMM");
        }
        }
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
    answer[ItemGroupInfo] = "groupType";
    answer[ItemGroupPeriod] = "grouping";
    // answer[SavedFiatRate] = "savedFiatRate";

    return answer;
}

QString WalletHistoryModel::dateForItem(qreal offset) const
{
    if (std::isnan(offset))
        return QString();
    const size_t row = std::round(offset * m_rowsProxy.size());
    if (row >= m_wallet->m_walletTransactions.size())
        return QString();
    auto item = m_wallet->m_walletTransactions.at(m_rowsProxy.at(row));
    if (item.minedBlockHeight <= 0)
        return QString();
    auto header = FloweePay::instance()->p2pNet()->blockchain().block(item.minedBlockHeight);
    if (header.nTime == 0)
        return QString();
    return QDateTime::fromSecsSinceEpoch(header.nTime).toString("MMMM yyyy");
}

void WalletHistoryModel::appendTransactions(int firstNew, int count)
{
    // lets assume sorting newest to oldest here.
    beginInsertRows(QModelIndex(), 0, count - 1);
    for (auto i = firstNew; i < firstNew + count; ++i) {
        m_rowsProxy.insert(0, i);
    }
    endInsertRows();
}

void WalletHistoryModel::transactionChanged(int txIndex)
{
    const int row = m_rowsProxy.indexOf(txIndex);
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

    TransactionGroupingResolver resolver;

    // we insert the key used in the m_wallet->m_walletTransaction map
    // in the order of how our rows work here.
    for (const auto &iter : m_wallet->m_walletTransactions) {
        if (!m_includeFlags.testFlag(WalletEnums::IncludeUnconfirmed)
                && iter.second.isUnconfirmed())
            continue;
        if (!m_includeFlags.testFlag(WalletEnums::IncludeRejected)
                && iter.second.isRejected())
            continue;
        if (!m_includeFlags.testFlag(WalletEnums::IncludeConfirmed)
                && !iter.second.isUnconfirmed())
            continue;
        m_rowsProxy.push_back(iter.first);
    }
    // the simplest form; reverse order. This assumes the last entry is the newest one
    std::reverse(m_rowsProxy.begin(), m_rowsProxy.end());

    // Last, resolve grouping
    for (int id : m_rowsProxy) {
        auto iter = m_wallet->m_walletTransactions.find(id);
        assert(iter != m_wallet->m_walletTransactions.end());
        resolver.add(iter->first, iter->second.minedBlockHeight);
    }

    // copy out the resolved groups to the lower-overhead ones.
    m_groups.clear();
    for (auto g : resolver.m_groups) {
        GroupInfo groupInfo;
        groupInfo.startTxIndex = g.startTxIndex;
        groupInfo.endTxIndex = g.endTxIndex;
        assert(g.period != TransactionGroup::Unset);
        static_assert(static_cast<WalletEnums::GroupingPeriod>(TransactionGroup::Week) == WalletEnums::Week);
        groupInfo.period = static_cast<WalletEnums::GroupingPeriod>(g.period);
        m_groups.push_back(groupInfo);
    }
    m_groupCache = { -1 , 0 };
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
    int index = 0;
    while (index < m_rowsProxy.size()) {
        const auto &tx = m_wallet->m_walletTransactions.find(m_rowsProxy.at(index));
        if (tx->second.minedBlockHeight < old)
            break;
        ++index;
    }
    // refresh the rows that need the 'new' indicator removed.
    if (index > 0) {
        beginRemoveRows(QModelIndex(), 0, index);
        endRemoveRows();
        beginInsertRows(QModelIndex(), 0, index);
        endInsertRows();
    }
}
