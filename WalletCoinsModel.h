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
#include <functional>

class Wallet;

class WalletCoinsModel : public QAbstractListModel
{
    Q_OBJECT
public:
    explicit WalletCoinsModel(Wallet *wallet, QObject *parent = nullptr);

    void setWallet(Wallet *newWallet);

    enum {
        Value = Qt::UserRole, // in sats
        Age,
        Blockheight,
        FusedCount,
        Locked, // bool
        Address,
        CloakedAddress,
        Selected
    };

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    /**
     * For a specific row return the Wallet::OutputRef that matches it.
     */
    uint64_t outRefForRow(int row) const;

    // lock or unlock a UTXO entry in the backing wallet.
    void setOutputLocked(int row, bool lock);

    void setSelectionGetter(const std::function<bool(uint64_t)> &callback);
    /// Find the row for a Wallet::OutputRef and mark it dirty for refresh in the UI
    void updateRow(uint64_t outRef);
    /// mark row dirty for refresh in the UI
    void updateRow(int row);

private slots:
    void uxosChanged();

private:
    void createMap();

    Wallet *m_wallet = nullptr;
    std::map<int, uint64_t> m_rowsToOutputRefs;

    std::function<bool(uint64_t)> m_selectionGetter;
};

#endif
