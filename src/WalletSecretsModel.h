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
#ifndef WALLETSECRETSMODEL_H
#define WALLETSECRETSMODEL_H

#include "Wallet.h"

#include <QAbstractListModel>
#include <QSet>
#include <map>

class WalletSecretsModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(bool showChangeChain READ showChangeChain WRITE setShowChangeChain NOTIFY showChangeChainChanged)
    Q_PROPERTY(bool showUsedAddresses READ showUsedAddresses WRITE setShowUsedAddresses NOTIFY showUsedAddressesChanged)
    Q_PROPERTY(QString search READ searchTerm WRITE search NOTIFY searchChanged)
public:
    explicit WalletSecretsModel(Wallet *wallet, QObject *parent = nullptr);

    enum {
        BitcoinAddress = Qt::DisplayRole,
        PrivateKey = Qt::UserRole,
        FromChangeChain,        ///< Bool. For HD wallets
        HDIndex,                ///< int, the derivation index
        Balance,                ///< long, the number of sats at this address
        HistoricalCoins,        ///< int, the number of historical coins touched this
        NumCoins,               ///< int, the number of coins still present on this address
        UsedSchnorr,            ///< bool, has transactions signed with Schnorr sigantures
        // UserLabel               ///< user-specified name
    };

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    QHash<int, QByteArray> roleNames() const override;

    /*
     * We show addresses based on these booleans:
     *   - is change-chain.
     *   - has been used
     *   - is search result
     * */

    bool showChangeChain() const;
    void setShowChangeChain(bool on);

    bool showUsedAddresses() const;
    void setShowUsedAddresses(bool on);

    void search(const QString &newSearch);
    const QString &searchTerm() const;

signals:
    void showChangeChainChanged();
    void showUsedAddressesChanged();
    void searchChanged();

private slots:
    void transactionsAddedToWallet(int from, int count);

private:
    Wallet *m_wallet;
    void update();
    const Wallet::KeyDetails &details(int privKeyId) const; // cached version of the details

    bool m_showChangeChain = false;
    bool m_showUsedAddresses = false;
    QString m_search;

    QVector<int> m_selectedPrivates;
    mutable QMutex m_lock;
    mutable std::map<int, Wallet::KeyDetails> m_keyDetails;
};

#endif
