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
#ifndef WALLETHSTORYMODEL_H
#define WALLETHSTORYMODEL_H

#include <QAbstractListModel>

class Wallet;

class WalletHistoryModel : public QAbstractListModel
{
    Q_OBJECT

public:
    explicit WalletHistoryModel(Wallet *wallet, QObject *parent = nullptr);

    enum {
        TxId = Qt::UserRole,
        MinedHeight,  ///< int, height of block this tx was mined at.
        MinedDate,    ///< A date-time object when the item was mined
        FundsIn,      ///< value (in sats) of the funds we own being spent
        FundsOut,     ///< value (in sats) of the outputs created we own
        WalletIndex,  ///< wallet-internal index for this transaction.

        Comment, // TODO (also: make editable)
        SavedFiatRate, // TODO
    };

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE QString dateForItem(qreal offset) const;

private slots:
    void appendTransactions(int firstNew, int count);
    void transactionConfirmed(int txIndex);

private:
    void createMap();

    QVector<int> m_rowsProxy;
    Wallet *m_wallet;
};

#endif
