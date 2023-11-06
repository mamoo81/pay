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
#include <QDate>
#include "Wallet.h"
#include "WalletEnums.h"

class Wallet;

class WalletHistoryModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int lastSyncIndicator READ lastSyncIndicator WRITE setLastSyncIndicator RESET resetLastSyncIndicator NOTIFY lastSyncIndicatorChanged)
    Q_PROPERTY(QFlags<WalletEnums::Include> includeFlags READ includeFlags WRITE setIncludeFlags NOTIFY includeFlagsChanged)
    /**
     * The view sometimes wants to ensure that content in the list does not move. For instance
     * when a popup related to one of the list-items is showing.
     * Freezing the model will stop the model from reporting new rows and un-freezing it aferwards
     * will cause all the new items that were found while frozen to be reported for UI update.
     */
    Q_PROPERTY(bool freezeModel READ isModelFrozen WRITE freezeModel NOTIFY freezeModelChanged)

public:
    explicit WalletHistoryModel(Wallet *wallet, QObject *parent = nullptr);

    enum {
        TxId = Qt::UserRole,
        NewTransaction, ///< Deprecated
        MinedHeight,  ///< int, height of block this tx was mined at.
        MinedDate,    ///< A date-time object when the item was mined
        FundsIn,      ///< value (in sats) of the funds we own being spent
        FundsOut,     ///< value (in sats) of the outputs created we own
        WalletIndex,  ///< wallet-internal index for this transaction.
        IsCoinbase,
        IsFused,
        Comment,
        PlacementInGroup,   ///< Is an enum WalletEnums::PlacementInGroup to help with painting outlines.
        GroupId             ///< The index in the m_groups vector
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
    Q_INVOKABLE QString groupingPeriod(int groupId) const;

    int lastSyncIndicator() const;
    void setLastSyncIndicator(int x);
    void resetLastSyncIndicator();

    const QFlags<WalletEnums::Include> &includeFlags() const;
    void setIncludeFlags(const QFlags<WalletEnums::Include> &flags);

    // Indicates a grouping of transactions, specifically based on a time-period
    struct TransactionGroup
    {
        WalletEnums::GroupingPeriod period = WalletEnums::Month;
        int startTxIndex = -1;
        int endTxIndex = -1;
        uint32_t endTime = 0;

        /// returns true if added, false if the tx-index is not for this group
        bool add(int txIndex, uint32_t timestamp, const QDate &today);
    };

    void freezeModel(bool on);
    bool isModelFrozen() const;

signals:
    void lastSyncIndicatorChanged();
    void includeFlagsChanged();
    void freezeModelChanged();

protected:
    // virtual to allow the unit test to not use p2pNet for this
    /// return the timestamp of a block (aka nTime) as defined by the block-header
    virtual uint32_t secsSinceEpochFor(int blockHeight) const;

    // only reimplemented in the unit test.
    virtual QDate today() const;

protected slots:
    void appendTransactions(int firstNew, int count);
    void removeTransaction(int txIndex);
    void transactionChanged(int txIndex);
    void createMap();

private:
    /// returns true if the filters indicate the transaction should stay.
    bool filterTransaction(const Wallet::WalletTransaction &wtx) const;
    /// Update m_groups to include this transaction.
    void addTxIndexToGroups(int txIndex, int blockheight);

    int txIndexFromRow(int row) const;

    /*
     * Our internal data-model.
     * This is a filter of the wallet-internal 'txIndex'. So we map index-in-vector to
     * transaction-index as known by the wallet.
     *
     * The main trick we use is that the vector starts with the oldest transactions while
     * our view starts with the newest. As such we simply invert the vector on insert/get.
     * (notice that we don't invert the actual vector since append-new would then
     *  become a costly insert-before).
     */
    QVector<int> m_rowsProxy;
    Wallet *m_wallet;
    QFlags<WalletEnums::Include> m_includeFlags = WalletEnums::IncludeAll;
    /*
     * To avoid weirdness in the assiging of transactions to groups based on the current date,
     * we keep the 'today' variable the same for the duration.
     */
    QDate m_today;

    std::vector<TransactionGroup> m_groups;
    struct GroupCache {
        int txIndex;
        int groupId;
    };
    int groupIdForTxIndex(int txIndex) const;
    mutable GroupCache m_groupCache = { -1, 0 };

    int m_lastSyncIndicator = 0;
    bool m_recreateTriggered = false;

    /// when above zero, we store the numbeer of rows we still need to process updates for
    /// while the listview has been frozen.
    /// See the freezeModel property for more.
    int m_rowsSilentlyInserted = -1;
};

#endif
