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
#ifndef PORTFOLIODATAPROVIDER_H
#define PORTFOLIODATAPROVIDER_H

#include <QObject>

#include "Wallet.h"
#include "WalletHistoryModel.h"

class AccountInfo : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int balance READ balance NOTIFY utxosChanged)
    Q_PROPERTY(int unspentOutputCount READ unspentOutputCount NOTIFY utxosChanged)
    Q_PROPERTY(int historicalOutputCount READ historicalOutputCount NOTIFY utxosChanged)
    Q_PROPERTY(int id READ id CONSTANT)
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(WalletHistoryModel* transactions READ historyModel CONSTANT)
public:
    AccountInfo(Wallet *wallet, QObject *parent = nullptr);

    /// the wallet-ID (aka the privacysegments segment-id). Application-unique.
    int id() const;

    /// return the current balance
    qint64 balance() const;
    /// return the amount of UTXOs that hold money
    int unspentOutputCount() const;
    /// return the amount of UTXOs ever created for this account.
    int historicalOutputCount() const;

    void setName(const QString &name);
    QString name() const;

    WalletHistoryModel* historyModel();

signals:
    void utxosChanged();
    void nameChanged();

private:
    Wallet *m_wallet;
    std::unique_ptr<WalletHistoryModel> m_model;
};

class PortfolioDataProvider : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QList<AccountInfo*> accounts READ accounts NOTIFY accountsChanged)
    Q_PROPERTY(AccountInfo* current READ current WRITE setCurrent NOTIFY currentChanged)
public:
    explicit PortfolioDataProvider(QObject *parent = nullptr);

    QList<AccountInfo*> accounts() const;

    AccountInfo *current() const;
    void setCurrent(AccountInfo *item);

public slots:
    void addWalletAccount(Wallet *wallet);

signals:
    void accountsChanged();
    void currentChanged();

private:
    QList<Wallet*> m_accounts;
    QList<AccountInfo*> m_accountInfos;

    int m_currentAccount = -1;
};

#endif
