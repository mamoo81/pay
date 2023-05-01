/*
 * This file is part of the Flowee project
 * Copyright (C) 2023 Tom Zander <tom@flowee.org>
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

#include <QObject>

class Wallet;

/**
 * This class goes together with a wallet and provides
 * a live view on transactions depositing money on a certain
 * private key, which has to be owned by the wallet.
 *
 * The Wallet::reserveUnusedAddress() method returns a
 * privagte key-index, you can set that with setPrivKeyIndex()
 * to activate this class.
 *
 * While a wallet is importing (see walletIsImporting()) all
 * transactions added are ignored as the point of this class
 * is to view the live changes, not (probably ancient) historical
 * ones.
 */
class WalletKeyView : public QObject
{
    Q_OBJECT
public:
    WalletKeyView(Wallet *wallet, QObject *parent);

    // set which private key of the viewed wallet we are filtering on.
    void setPrivKeyIndex(int privKeyId);

    bool walletIsImporting() const;

    enum UTXOState {
        UTXOSeen,
        UTXOConfirmed,
        UTXORejected
    };

    struct Transaction {
        UTXOState state = UTXOSeen;
        uint64_t ref = 0;
        uint64_t amount = 0;
    };

    QList<Transaction> transactions() const;

signals:
    void transactionMatch(uint64_t ref, uint64_t amount, UTXOState state);
    void walletEncrypted();
    void importFinished();

private slots:
    void appendedTransactions(int firstNew, int count);
    void transactionConfirmed(int txIndex);
    void encryptionChanged();
    void lastBlockSynchedChanged();

private:
    const Wallet * const m_wallet;
    // set at startup to true if the wallet is in the (once in its lifetime) importing
    // stage. If true, all deposits are historical!
    bool m_walletIsImporting;
    int m_privKeyIndex = -1;

    QList<Transaction> m_transactions;
};
