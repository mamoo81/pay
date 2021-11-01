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
#include "WalletHistoryModel.h"
#include "Wallet.h"
#include "FloweePay.h"

#include <BlockHeader.h>

#include <QLocale>
#include <QDateTime>

WalletHistoryModel::WalletHistoryModel(Wallet *wallet, QObject *parent)
    : QAbstractListModel(parent),
      m_wallet(wallet)
{
    QMutexLocker locker(&m_wallet->m_lock);
    assert(wallet->segment());
    resetLastSyncIndicator();
    createMap();

    connect(wallet, SIGNAL(appendedTransactions(int,int)), SLOT(appendTransactions(int,int)), Qt::QueuedConnection);
    connect(wallet, SIGNAL(transactionConfirmed(int)), SLOT(transactionConfirmed(int)), Qt::QueuedConnection);
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
    auto item = m_wallet->m_walletTransactions.at(m_rowsProxy.at(index.row()));
    switch (role) {
    case TxId:
        return QVariant(QString::fromStdString(item.txid.ToString()));
    case MinedHeight:
        return QVariant(item.minedBlockHeight);
    case MinedDate: {
        if (item.minedBlockHeight <= 0)
            return QVariant();
        auto header = FloweePay::instance()->p2pNet()->blockchain().block(item.minedBlockHeight);
        return QVariant(QDateTime::fromTime_t(header.nTime)); // .toString(format));
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
        return QVariant(item.minedBlockHeight > m_lastSyncIndicator);
    }

    return QVariant();
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
    // answer[Comment] = "comment";
    // answer[SavedFiatRate] = "savedFiatRate";

    return answer;
}

QString WalletHistoryModel::dateForItem(qreal offset) const
{
    if (std::isnan(offset))
        return QString();
    const size_t row = std::round(offset * m_rowsProxy.size());
    if (row < 0 || row >= m_wallet->m_walletTransactions.size())
        return QString();
    auto item = m_wallet->m_walletTransactions.at(m_rowsProxy.at(row));
    if (item.minedBlockHeight <= 0)
        return QString();
    auto header = FloweePay::instance()->p2pNet()->blockchain().block(item.minedBlockHeight);
    if (header.nTime == 0)
        return QString();
    return QDateTime::fromTime_t(header.nTime).toString("MMMM yyyy");
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

void WalletHistoryModel::transactionConfirmed(int txIndex)
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
    m_rowsProxy.clear();
    m_rowsProxy.resize(m_wallet->m_walletTransactions.size());

    // we insert the key used in the m_wallet->m_walletTransaction map
    // in the order of how our rows work here.

    // the simplest form; reverse order. This assumes the last entry is the newest one
    int i = m_rowsProxy.size() - 1;
    for (const auto &iter : m_wallet->m_walletTransactions) {
        assert(i >= 0);
        m_rowsProxy[i--] = iter.first;
    }
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
