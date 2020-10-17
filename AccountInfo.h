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
#ifndef ACCOUNTINFO_H
#define ACCOUNTINFO_H

#include <QObject>
#include "WalletHistoryModel.h"
#include <memory>

class Wallet;

class AccountInfo : public QObject
{
    Q_OBJECT
    /// The wallet amount
    Q_PROPERTY(double balance READ balance NOTIFY utxosChanged)
    Q_PROPERTY(int unspentOutputCount READ unspentOutputCount NOTIFY utxosChanged)
    Q_PROPERTY(int historicalOutputCount READ historicalOutputCount NOTIFY utxosChanged)
    Q_PROPERTY(int id READ id CONSTANT)
    Q_PROPERTY(int lastBlockSynched READ lastBlockSynched NOTIFY lastBlockSynchedChanged)
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(WalletHistoryModel* transactions READ historyModel CONSTANT)
public:
    AccountInfo(Wallet *wallet, QObject *parent = nullptr);

    /// the wallet-ID (aka the privacysegments segment-id). Application-unique.
    int id() const;

    /// return the current balance
    double balance() const;

    /// return the amount of UTXOs that hold money
    int unspentOutputCount() const;
    /// return the amount of UTXOs ever created for this account.
    int historicalOutputCount() const;

    void setName(const QString &name);
    QString name() const;

    int lastBlockSynched() const;

    WalletHistoryModel* historyModel();

signals:
    void utxosChanged();
    void nameChanged();
    void lastBlockSynchedChanged();

private:
    Wallet *m_wallet;
    std::unique_ptr<WalletHistoryModel> m_model;
};

#endif
