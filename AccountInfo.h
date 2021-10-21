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
#ifndef ACCOUNTINFO_H
#define ACCOUNTINFO_H

#include <QObject>
#include "WalletHistoryModel.h"
#include "Wallet.h"
#include "TransactionInfo.h"
#include <PaymentRequest.h>
#include <memory>

class TransactionInfo;

class WalletSecret : public QObject
{
    Q_OBJECT
    /// The wallet-internal ID
    Q_PROPERTY(int id READ id CONSTANT)
    /// If this has been used yet.
    Q_PROPERTY(bool used READ used WRITE setUsed NOTIFY usedChanged)
    /// When used, was it used to sign with Schnorr
    Q_PROPERTY(bool usedSchnorr READ usedSchnorr WRITE setUsedSchnorr NOTIFY usedSchnorrChanged)
    /// The human readable address
    Q_PROPERTY(QString address READ address WRITE setAddress NOTIFY addressChanged)
    /// the amount of satoshis currently associated with this secret
    Q_PROPERTY(qint64 saldo READ saldo WRITE setSaldo NOTIFY saldoChanged)
public:
    explicit WalletSecret(int id, QObject *parent = nullptr);

    int id() const;

    bool used() const;
    void setUsed(bool newUsed);

    bool usedSchnorr() const;
    void setUsedSchnorr(bool newUsedSchnorr);

    const QString &address() const;
    void setAddress(const QString &newAddress);

    qint64 saldo() const;
    void setSaldo(qint64 newSaldo);

    // \internal
    void populate(const Wallet::WalletSecret &secret, qint64 saldo);

    Q_INVOKABLE QString fetchPrivateKey() const;

signals:
    void usedChanged();
    void usedSchnorrChanged();
    void addressChanged();
    void saldoChanged();

private:
    const int m_id;
    bool m_used = false;
    bool m_usedSchnorr = false;
    QString m_address;
    qint64 m_saldo = 0;
};

class AccountInfo : public QObject
{
    Q_OBJECT
    /// The wallet amount
    Q_PROPERTY(double balanceConfirmed READ balanceConfirmed NOTIFY balanceChanged)
    Q_PROPERTY(double balanceUnconfirmed READ balanceUnconfirmed NOTIFY balanceChanged)
    Q_PROPERTY(double balanceImmature READ balanceImmature NOTIFY balanceChanged)
    Q_PROPERTY(int unspentOutputCount READ unspentOutputCount NOTIFY utxosChanged)
    Q_PROPERTY(int historicalOutputCount READ historicalOutputCount NOTIFY utxosChanged)
    Q_PROPERTY(int id READ id CONSTANT)
    Q_PROPERTY(int lastBlockSynched READ lastBlockSynched NOTIFY lastBlockSynchedChanged)
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(WalletHistoryModel* transactions READ historyModel CONSTANT)
    Q_PROPERTY(bool isDefaultWallet READ isDefaultWallet WRITE setDefaultWallet NOTIFY isDefaultWalletChanged)
    Q_PROPERTY(bool isUserOwned READ userOwnedWallet NOTIFY userOwnedChanged)
    Q_PROPERTY(bool isSingleAddressAccount READ isSingleAddressAccount CONSTANT)
    Q_PROPERTY(bool isHDAccount READ isHDAccount CONSTANT)
    Q_PROPERTY(QList<QObject*> paymentRequests READ paymentRequests NOTIFY paymentRequestsChanged)
    Q_PROPERTY(QList<WalletSecret *> walletSecrets READ walletSecrets NOTIFY walletSecretsChanged)
    Q_PROPERTY(QString mnemonic READ hdWalletMnemonic CONSTANT)
    Q_PROPERTY(QString hdDerivationPath READ hdDerivationPath CONSTANT)
public:
    AccountInfo(Wallet *wallet, QObject *parent = nullptr);

    /// the wallet-ID (aka the privacysegments segment-id). Application-unique.
    int id() const;

    /// return the current balance, but only confirmed and spendable
    double balanceConfirmed() const;

    /// return the unconfirmed balance, only not yet confirmed outputs are counted
    double balanceUnconfirmed() const;

    /// Return the balanace of coinbases we are not allowed to spend yet
    double balanceImmature() const;

    /// return the amount of UTXOs that hold money
    int unspentOutputCount() const;
    /// return the amount of UTXOs ever created for this account.
    int historicalOutputCount() const;

    void setName(const QString &name);
    QString name() const;

    int lastBlockSynched() const;

    WalletHistoryModel* historyModel();

    /**
     * Sets a wallet to be the first to open (aka default) wallet.
     */
    void setDefaultWallet(bool isDefault);
    /// returns if a wallet is the main, first, wallet
    bool isDefaultWallet();

    // maps to Wallet::userOwnedWallet
    bool userOwnedWallet();

    /**
     * All payment requests that are created for this account.
     */
    QList<QObject*> paymentRequests() const;

    Q_INVOKABLE TransactionInfo* txInfo(int walletIndex, QObject *parent);
    /**
     * Start a new payment-request
     * QML callers should pass a panel as parent, preferably one that is loaded.
     */
    Q_INVOKABLE QObject* createPaymentRequest(QObject *parent);

    Wallet *wallet() const {
        return m_wallet;
    }

    bool isSingleAddressAccount() const;

    bool isHDAccount() const;
    QString hdWalletMnemonic() const;
    QString hdDerivationPath() const;

    const QList<WalletSecret *> &walletSecrets();

signals:
    void balanceChanged();
    void utxosChanged();
    void nameChanged();
    void lastBlockSynchedChanged();
    void isDefaultWalletChanged();
    void paymentRequestsChanged();
    void userOwnedChanged();
    void walletSecretsChanged();

private slots:
    void updateWalletSecret(int id);

private:
    Wallet *m_wallet;
    std::unique_ptr<WalletHistoryModel> m_model;
    QList<WalletSecret*> m_walletSecrets;

    friend class WalletSecret;
};

#endif
