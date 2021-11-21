/*
 * This file is part of the Flowee project
 * Copyright (C) 2021 Tom Zander <tom@flowee.org>
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
#ifndef WALLETCOINSMODEL_H
#define WALLETCOINSMODEL_H

#include <QAbstractListModel>

class Wallet;

class WalletCoinsModel : public QAbstractListModel
{
    Q_OBJECT
public:
    explicit WalletCoinsModel(Wallet *wallet, QObject *parent = nullptr);

    void setWallet(Wallet *newWallet);

    enum {
        Value = Qt::UserRole, // in sats
        Blockheight,
        FusedCount,
        Address,
        CloakedAddress
    };

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

private slots:
    void uxosChanged();

private:
    void createMap();

    Wallet *m_wallet;
    std::map<int, uint64_t> m_rowsToOutputRefs;
};

#endif
