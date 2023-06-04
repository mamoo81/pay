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

#include "WalletCoinsModel.h"
#include "Wallet.h"
#include "FloweePay.h"
#include <p2p/DownloadManager.h>

#include <QMutex>
#include <cashaddr.h>


WalletCoinsModel::WalletCoinsModel(Wallet *wallet, QObject *parent)
    : QAbstractListModel(parent)
{
    setWallet(wallet);
}

void WalletCoinsModel::setWallet(Wallet *newWallet)
{
    assert(newWallet);
    assert(newWallet->segment());
    if (m_wallet == newWallet)
        return;
    if (m_wallet)
        disconnect (m_wallet, SIGNAL(utxosChanged()), this, SLOT(utxosChanged()));
    m_wallet = newWallet;
    connect (m_wallet, SIGNAL(utxosChanged()), this, SLOT(utxosChanged()));

    beginRemoveRows(QModelIndex(), 0, m_rowsToOutputRefs.size() - 1);
    endRemoveRows();
    createMap();
    beginInsertRows(QModelIndex(), 0, m_rowsToOutputRefs.size() - 1);
    endInsertRows();
}

int WalletCoinsModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) // only for the (invalid) root node we return a count, since this is a list not a tree
        return 0;

    return m_rowsToOutputRefs.size();
}

QVariant WalletCoinsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();


    auto iter = m_rowsToOutputRefs.find(index.row());
    if (iter == m_rowsToOutputRefs.end()) {
        logDebug(LOG_WALLET) << "This looks wrong";
        return QVariant();
    }
    Wallet::OutputRef outRef(iter->second);
    QMutexLocker locker(&m_wallet->m_lock);
    switch (role) {
    case Value: {
        const auto utxo = m_wallet->m_unspentOutputs.find(iter->second);
        if (utxo != m_wallet->m_unspentOutputs.end())
            return QVariant(static_cast<double>(utxo->second));
        break;
    }
    case Age: {
        const auto tx = m_wallet->m_walletTransactions.find(outRef.txIndex());
        if (tx != m_wallet->m_walletTransactions.end()) {
            const int bh = tx->second.minedBlockHeight;
            if (bh < 1)
                return tr("Unconfirmed");
            const int now = FloweePay::instance()->p2pNet()->blockHeight();
            const auto diff = now - bh;
            if (diff < 76) {
                const int hours = diff / 6;
                return tr("%1 hours", "age, like: hours old", hours + 1).arg(hours + 1);
            }
            const int days = (diff - 20) / 144;
            if (days < 10)
                return tr("%1 days", "age, like: days old", days + 1).arg(days + 1);
            if (days < 6 * 7) {
                const int weeks = (days - 2) / 7;
                return tr("%1 weeks", "age, like: weeks old", weeks).arg(weeks);
            }
            const int months = qRound((days - 2) / 30.4);
            return tr("%1 months", "age, like: months old", months).arg(months);
        }
        break;
    }
    case Blockheight: {
        const auto tx = m_wallet->m_walletTransactions.find(outRef.txIndex());
        if (tx != m_wallet->m_walletTransactions.end())
            return QVariant(tx->second.minedBlockHeight);
        break;
    }
    case FusedCount: {
        const auto tx = m_wallet->m_walletTransactions.find(outRef.txIndex());
        if (tx != m_wallet->m_walletTransactions.end())
            return QVariant(tx->second.isCashFusionTx ? 1 : 0);
        break;
    }
    case Address: {
        const auto tx = m_wallet->m_walletTransactions.find(outRef.txIndex());
        if (tx != m_wallet->m_walletTransactions.end()) {
            auto out = tx->second.outputs.find(outRef.outputIndex());
            assert(out != tx->second.outputs.end());
            auto secretIter = m_wallet->m_walletSecrets.find(out->second.walletSecretId);
            assert(secretIter != m_wallet->m_walletSecrets.end());
            const auto &secret = secretIter->second;
            return QVariant(renderAddress(secret.address));
        }
        break;
    }
    case CloakedAddress: {
        if (m_wallet->isSingleAddressWallet())
            return QVariant(m_wallet->name());
        const auto tx = m_wallet->m_walletTransactions.find(outRef.txIndex());
        if (tx != m_wallet->m_walletTransactions.end()) {
            auto out = tx->second.outputs.find(outRef.outputIndex());
            assert(out != tx->second.outputs.end());
            auto secretIter = m_wallet->m_walletSecrets.find(out->second.walletSecretId);
            assert(secretIter != m_wallet->m_walletSecrets.end());
            const auto &secret = secretIter->second;
            if (secret.fromHdWallet && secret.fromChangeChain)
                return QVariant(tr("Change #%1").arg(secret.hdDerivationIndex));
        }
        break;
    }
    case Locked: {
        const auto lockedIter = m_wallet->m_lockedOutputs.find(iter->second);
        return QVariant(lockedIter != m_wallet->m_lockedOutputs.end());
    }
    case Selected: {
        if (m_selectionGetter)
            return QVariant(m_selectionGetter(iter->second));
    }
    default:
        break;
    }
    return QVariant();
}

QHash<int, QByteArray> WalletCoinsModel::roleNames() const
{
    QHash<int, QByteArray> answer;
    answer[Value] = "value";
    answer[Blockheight] = "blockheight";
    answer[Age] = "age";
    answer[FusedCount] = "fusedCount";
    answer[Address] = "address";
    answer[CloakedAddress] = "cloakedAddress";
    answer[Locked] = "locked";
    answer[Selected] = "selected";
    return answer;
}

uint64_t WalletCoinsModel::outRefForRow(int row) const
{
    auto i = m_rowsToOutputRefs.find(row);
    if (i == m_rowsToOutputRefs.end())
        return 0;
    return i->second;
}

void WalletCoinsModel::setOutputLocked(int row, bool lock)
{
    auto i = m_rowsToOutputRefs.find(row);
    if (i == m_rowsToOutputRefs.end())
        return;
    const Wallet::OutputRef id(i->second);
    bool changed;
    if (lock)
        changed = m_wallet->lockUTXO(id);
    else
        changed = m_wallet->unlockUTXO(id);

    if (changed) {
        beginRemoveRows(QModelIndex(), row, row);
        endRemoveRows();
        beginInsertRows(QModelIndex(), row, row);
        endInsertRows();
    }
}

void WalletCoinsModel::setSelectionGetter(const std::function<bool (uint64_t)> &callback)
{
    m_selectionGetter = callback;
}

void WalletCoinsModel::updateRow(uint64_t outRef)
{
    for (auto i = m_rowsToOutputRefs.cbegin(); i != m_rowsToOutputRefs.cend(); ++i) {
        if (i->second == outRef) {
            updateRow(i->first);
            break;
        }
    }
}

void WalletCoinsModel::updateRow(int row)
{
    beginRemoveRows(QModelIndex(), row, row);
    endRemoveRows();
    beginInsertRows(QModelIndex(), row, row);
    endInsertRows();
}

void WalletCoinsModel::utxosChanged()
{
    beginRemoveRows(QModelIndex(), 0, m_rowsToOutputRefs.size());
    endRemoveRows();
    createMap();
    beginInsertRows(QModelIndex(), 0, m_rowsToOutputRefs.size());
    endInsertRows();
}

void WalletCoinsModel::createMap()
{
    QMutexLocker locker(&m_wallet->m_lock); // yes, its a recursive lock
    m_rowsToOutputRefs.clear();

    int index = 0;
    for (auto i = m_wallet->m_unspentOutputs.crbegin(); i != m_wallet->m_unspentOutputs.rend(); ++i) {
        auto lockIter = m_wallet->m_lockedOutputs.find(i->first);
        if (lockIter != m_wallet->m_lockedOutputs.end()) {
            // its locked, so we check if its locked by a transaction not yet confirmed.
            // If it is (and the second points to a wallet-transaction-index) we simply
            // hide this one from view.
            if (lockIter->second > 0)
                continue;
        }

        Wallet::OutputRef ref(i->first);
        auto wtxIter = m_wallet->m_walletTransactions.find(ref.txIndex());
        assert(wtxIter != m_wallet->m_walletTransactions.end());
        const auto &wtx = wtxIter->second;
        const auto outputIter = wtx.outputs.find(ref.outputIndex());
        assert(outputIter != wtx.outputs.end());
        if (outputIter->second.holdsCashToken) {
            // CashToken holding UTXOs are hidden until we figure out how they can be handled in a future release.
            continue;
        }

        m_rowsToOutputRefs.insert(std::make_pair(index++, i->first));
    }
}


