/*
 * This file is part of the Flowee project
 * Copyright (C) 2023-2024 Tom Zander <tom@flowee.org>
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

#include "WalletKeyView.h"
#include "Wallet.h"

#include <QThread>

WalletKeyView::WalletKeyView(Wallet *wallet, QObject *parent)
    : QObject(parent),
      m_wallet(wallet)
{
    assert(wallet);
    m_walletIsImporting = wallet->walletIsImporting();

    // connect all.
    // Notice that the wallet emits those in any thread. So we only use
    // queued connections here.
    connect(wallet, SIGNAL(appendedTransactions(int,int)), this,
            SLOT(appendedTransactions(int,int)), Qt::QueuedConnection);
    connect(wallet, SIGNAL(lastBlockSynchedChanged()), this,
            SLOT(lastBlockSynchedChanged()), Qt::QueuedConnection);
    connect(wallet, SIGNAL(transactionConfirmed(int)), this,
            SLOT(transactionConfirmed(int)), Qt::QueuedConnection);
    connect(wallet, SIGNAL(encryptionChanged()), this,
            SLOT(encryptionChanged()), Qt::QueuedConnection);
}

void WalletKeyView::setPrivKeyIndex(int privKeyIndex)
{
    m_privKeyIndex = privKeyIndex;
}

void WalletKeyView::appendedTransactions(const int firstNew, const int count)
{
    if (m_walletIsImporting)
        return;
    assert(QThread::currentThread() == thread());
    QMutexLocker locker(&m_wallet->m_lock);
    logInfo() << " getting new tx. Index:" << firstNew << "count:" << count;
    for (int txIndex = firstNew; txIndex < firstNew + count; ++txIndex) {
        auto itemIter = m_wallet->m_walletTransactions.find(txIndex);
        if (itemIter == m_wallet->m_walletTransactions.end()) // the wallet changed, we're probably waiting for a queued connection
            continue;
        for (auto output : itemIter->second.outputs) {
            if (output.second.walletSecretId == m_privKeyIndex) {
               Transaction tx;
                tx.ref = Wallet::OutputRef(txIndex, output.first).encoded();
                tx.state = UTXOSeen;
                const auto blockHeight = itemIter->second.minedBlockHeight;
                if (blockHeight > 0)
                    tx.state = UTXOConfirmed;
                else if (blockHeight == -2)
                    tx.state = UTXORejected;
                tx.amount = output.second.value;
                bool exists = false;
                for (auto txIter = m_transactions.begin(); txIter != m_transactions.end(); ++txIter) {
                    if (txIter->ref == tx.ref) {
                        exists = true;
                        txIter->state = tx.state;
                        assert(txIter->amount == tx.amount);
                    }
                }
                if (!exists)
                    m_transactions.append(tx);
                emit transactionMatch(tx.ref, tx.amount, tx.state);
            }
        }
    }
}

void WalletKeyView::lastBlockSynchedChanged()
{
    assert(QThread::currentThread() == thread());
    if (m_walletIsImporting && !m_wallet->walletIsImporting()) {
        m_walletIsImporting = false;
        emit importFinished();
    }
}

QList<WalletKeyView::Transaction> WalletKeyView::transactions() const
{
    return m_transactions;
}

void WalletKeyView::transactionConfirmed(int txIndex)
{
    assert(QThread::currentThread() == thread());
    if (m_walletIsImporting)
        return;
    auto refBase = Wallet::OutputRef(txIndex, 0).encoded();
    for (auto txIter = m_transactions.begin(); txIter != m_transactions.end(); ++txIter) {
        if (refBase == (txIter->ref & 0xFFFFFFFFFFFF0000)) {
            if (txIter->state != UTXOConfirmed) {
                txIter->state = UTXOConfirmed;
                emit transactionMatch(txIter->ref, txIter->amount, UTXOConfirmed);
            }
        }
    }
}

void WalletKeyView::encryptionChanged()
{
    assert(QThread::currentThread() == thread());
    if (m_wallet->encryption() == Wallet::FullyEncrypted && !m_wallet->isDecrypted())
        emit walletEncrypted();
}
