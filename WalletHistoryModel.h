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
#ifndef WALLETHSTORYMODEL_H
#define WALLETHSTORYMODEL_H

#include <QAbstractListModel>
#include "WalletEnums.h"

class Wallet;

class WalletHistoryModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int lastSyncIndicator READ lastSyncIndicator WRITE setLastSyncIndicator RESET resetLastSyncIndicator NOTIFY lastSyncIndicatorChanged)
    Q_PROPERTY(QFlags<WalletEnums::Include> includeFlags READ includeFlags WRITE setIncludeFlags NOTIFY includeFlagsChanged)

public:
    explicit WalletHistoryModel(Wallet *wallet, QObject *parent = nullptr);

    enum {
        TxId = Qt::UserRole,
        NewTransaction, ///< bool, if this transaction is newer than lastSyncIndicator
        MinedHeight,  ///< int, height of block this tx was mined at.
        MinedDate,    ///< A date-time object when the item was mined
        FundsIn,      ///< value (in sats) of the funds we own being spent
        FundsOut,     ///< value (in sats) of the outputs created we own
        WalletIndex,  ///< wallet-internal index for this transaction.
        IsCoinbase,
        IsCashFusion,
        Comment,
        ItemGroupInfo,
        ItemGroupType,
        // SavedFiatRate, // TODO
    };

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    /**
     * Return a formatted date for an item in our list.
     * @param offset a real number that is between 0.0 and 1.0, indicating the location of the item.
     *        The value 0.0 is the most recent item.
     */
    Q_INVOKABLE QString dateForItem(qreal offset) const;

    int lastSyncIndicator() const;
    void setLastSyncIndicator(int x);
    void resetLastSyncIndicator();

    const QFlags<WalletEnums::Include> &includeFlags() const;
    void setIncludeFlags(const QFlags<WalletEnums::Include> &flags);

signals:
    void lastSyncIndicatorChanged();
    void includeFlagsChanged();

private slots:
    void appendTransactions(int firstNew, int count);
    void transactionChanged(int txIndex);
    void createMap();

private:
    QVector<int> m_rowsProxy;
    Wallet *m_wallet;
    QFlags<WalletEnums::Include> m_includeFlags = WalletEnums::IncludeAll;

    struct GroupInfo {
        WalletEnums::GroupingPeriod period;
        int startTxIndex;
        int endTxIndex;
    };
    std::vector<GroupInfo> m_groups;

    int m_lastSyncIndicator = 0;
    bool m_recreateTriggered = false;
};

#endif
