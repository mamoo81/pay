/*
 * This file is part of the Flowee project
 * Copyright (C) 2020 Tom Zander <tomz@freedommail.ch>
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
    createMap();

    connect(wallet, SIGNAL(appendedTransactions(int,int)), SLOT(appendTransactions(int,int)), Qt::QueuedConnection);
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
        auto header = FloweePay::instance()->p2pNet()->blockchain().block(item.minedBlockHeight);
        auto format = QLocale::system().dateFormat(QLocale::ShortFormat);
        return QVariant(QDateTime::fromTime_t(header.nTime).toString(format));
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
        return QVariant(value);
    }
    case FundsOut: {
        qint64 value = 0;
        for (auto out : item.outputs) {
            value += out.second.value;
        }
        return QVariant(value);
    }
    case Unspent:
        // TODO
        return QVariant(false);
    }

    return QVariant();
}


QHash<int, QByteArray> WalletHistoryModel::roleNames() const
{
    QHash<int, QByteArray> answer;
    answer[TxId] = "txid";
    answer[MinedHeight] = "height";
    answer[MinedDate] = "date";
    answer[FundsIn] = "fundsIn";
    answer[FundsOut] = "fundsOut";
    answer[Unspent] = "unspent";
    // answer[Comment] = "comment";
    // answer[SavedFiatRate] = "savedFiatRate";

    return answer;
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

void WalletHistoryModel::createMap()
{
    m_rowsProxy.clear();
    m_rowsProxy.resize(m_wallet->m_walletTransactions.size());

    // we insert the key used in the m_wallet->m_walletTransaction map
    // in the order of how our rows work here.

    // the simplest form; reverse order. This assumes the last entry is the newest one
    int i = m_rowsProxy.size() - 1;
    for (auto iter : m_wallet->m_walletTransactions) {
        assert(i >= 0);
        m_rowsProxy[i--] = iter.first;
    }
}
