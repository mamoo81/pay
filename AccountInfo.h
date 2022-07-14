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
#ifndef ACCOUNTINFO_H
#define ACCOUNTINFO_H

#include <QObject>
#include "PaymentRequest.h"
#include "TransactionInfo.h"
#include "Wallet.h"
#include "WalletHistoryModel.h"
#include "WalletSecretsModel.h"
#include "qtimer.h"

#include <QDateTime>
#include <memory>

class TransactionInfo;

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
    Q_PROPERTY(QDateTime lastBlockSynchedTime READ lastBlockSynchedTime NOTIFY lastBlockSynchedChanged)
    /// Return a user-readable indication of the amount of time 'behind' this account is
    Q_PROPERTY(QString timeBehind READ timeBehind NOTIFY lastBlockSynchedChanged)
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(WalletHistoryModel* transactions READ historyModel NOTIFY modelsChanged)
    Q_PROPERTY(WalletSecretsModel* secrets READ secretsModel NOTIFY modelsChanged)
    Q_PROPERTY(bool isDefaultWallet READ isDefaultWallet WRITE setDefaultWallet NOTIFY isDefaultWalletChanged)
    Q_PROPERTY(bool isUserOwned READ userOwnedWallet NOTIFY userOwnedChanged)
    Q_PROPERTY(bool isSingleAddressAccount READ isSingleAddressAccount CONSTANT)
    Q_PROPERTY(bool isHDWallet READ isHDWallet CONSTANT)
    Q_PROPERTY(bool isArchived READ isArchived WRITE setIsArchived NOTIFY isArchivedChanged)
    Q_PROPERTY(QList<QObject*> paymentRequests READ paymentRequests NOTIFY paymentRequestsChanged)
    Q_PROPERTY(QString mnemonic READ hdWalletMnemonic CONSTANT)
    Q_PROPERTY(QString hdDerivationPath READ hdDerivationPath CONSTANT)
    Q_PROPERTY(QDateTime lastMinedTransaction READ lastMinedTransaction NOTIFY balanceChanged)
    Q_PROPERTY(bool hasFreshTransactions READ hasFreshTransactions WRITE setHasFreshTransactions NOTIFY hasFreshTransactionsChanged)
    Q_PROPERTY(bool needsPinToPay READ needsPinToPay NOTIFY encryptionChanged)
    Q_PROPERTY(bool needsPinToOpen READ needsPinToOpen NOTIFY encryptionChanged)
    Q_PROPERTY(bool isDecrypted READ isDecrypted NOTIFY encryptionChanged)
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
    QDateTime lastBlockSynchedTime() const;
    QString timeBehind() const;

    WalletHistoryModel* historyModel();
    WalletSecretsModel* secretsModel();

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

    Q_INVOKABLE void encryptPinToPay(const QString &password);
    Q_INVOKABLE void encryptPinToOpen(const QString &password);
    Q_INVOKABLE bool decrypt(const QString &password);
    /// Remove all secrets from the encrypted wallet.
    Q_INVOKABLE void closeWallet();

    Wallet *wallet() const {
        return m_wallet;
    }

    bool isSingleAddressAccount() const;

    /// return true if the wallet is based on a master key and mnemonic seed.
    bool isHDWallet() const;
    /// Return the mnemonic seed that is the basis of this wallet.
    QString hdWalletMnemonic() const;
    /// Return the derivation base path that is the basis of this wallet.
    QString hdDerivationPath() const;

    /// Return the model that shares more info about the wallets private keys
    WalletSecretsModel *secretsModel() const;

    /// Returns the date of the last timestamped transaction
    QDateTime lastMinedTransaction() const;

    bool isArchived() const;
    void setIsArchived(bool newIsArchived);

    bool hasFreshTransactions() const;
    void setHasFreshTransactions(bool fresh);

    bool needsPinToPay() const;
    bool needsPinToOpen() const;
    bool isDecrypted() const;

signals:
    void balanceChanged();
    void utxosChanged();
    void nameChanged();
    void lastBlockSynchedChanged();
    void timeBehindChanged();
    void isDefaultWalletChanged();
    void paymentRequestsChanged();
    void userOwnedChanged();
    void isArchivedChanged();
    void hasFreshTransactionsChanged();
    void encryptionChanged();
    void modelsChanged();

private slots:
    // callback from wallet
    void balanceHasChanged();
    void walletEncryptionChanged();

private:
    Wallet * const m_wallet;
    QTimer *m_closeWalletTimer = nullptr;
    std::unique_ptr<WalletHistoryModel> m_model;
    std::unique_ptr<WalletSecretsModel> m_secretsModel;
    int m_lastTxHeight = -1; ///< last seen tx blockheight.
    bool m_hasFreshTransactions = false;

    friend class WalletSecret;
};

#endif
