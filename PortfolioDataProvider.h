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

#include <TransactionBuilder.h>
#include <BroadcastTxData.h>

class AccountInfo : public QObject
{
    Q_OBJECT
    /// The wallet amount
    Q_PROPERTY(double balance READ balance NOTIFY utxosChanged)
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
    double balance() const;

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


class Payment : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int feePerByte READ feePerByte WRITE setFeePerByte NOTIFY feePerByteChanged)
    Q_PROPERTY(qint64 paymentAmount READ paymentAmount WRITE setPaymentAmount NOTIFY amountChanged)
    Q_PROPERTY(QString targetAddress READ targetAddress WRITE setTargetAddress NOTIFY targetAddressChanged)
public:
    Payment(Wallet *wallet);

    void setFeePerByte(int sats);
    int feePerByte();

    void setPaymentAmount(qint64 amount);
    qint64 paymentAmount();

    /// this method throws if its not a proper address.
    /// @see FloweePay::identifyString()
    void setTargetAddress(const QString &address);
    QString targetAddress();

    Q_INVOKABLE void approveAndSend();

    Wallet *wallet() const;

private slots:
    void sentToPeer();
    void txRejected(short reason, const QString &message);

signals:
    void feePerByteChanged();
    void amountChanged();
    void targetAddressChanged();
    /// notify how many peers we relayed the transaction to.
    void sent(int count);

private:
    Wallet *m_wallet;
    int m_fee; // in sats per byte
    qint64 m_paymentAmount;
    QString m_address;
    std::shared_ptr<BroadcastTxData> m_infoObject;
    short m_sentPeerCount = 0;
    short m_rejectedPeerCount = 0;
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

    Q_INVOKABLE QObject* startPayToAddress(const QString &address, qint64 amount);

    void selectDefaultWallet();

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
