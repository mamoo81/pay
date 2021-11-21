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

#include <QMutex>
#include <cashaddr.h>


WalletCoinsModel::WalletCoinsModel(Wallet *wallet, QObject *parent)
    : QAbstractListModel(parent),
    m_wallet(wallet)
{
    assert(wallet);
    QMutexLocker locker(&m_wallet->m_lock);
    assert(wallet->segment());
    createMap();

    connect (m_wallet, SIGNAL(utxosChanged()), this, SLOT(uxosChanged()));
}

void WalletCoinsModel::setWallet(Wallet *newWallet)
{
    assert(newWallet);
    if (m_wallet == newWallet)
        return;
    m_wallet = newWallet;
    // TODO update rows
    createMap();
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
        logDebug() << "This looks wrong";
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
    answer[FusedCount] = "fusedCount";
    answer[Address] = "address";
    answer[CloakedAddress] = "cloakedAddress";
    return answer;
}

void WalletCoinsModel::uxosChanged()
{
    // TODO
    logFatal() << "do something";
}

void WalletCoinsModel::createMap()
{
    QMutexLocker locker(&m_wallet->m_lock); // yes, its a recursive lock
    m_rowsToOutputRefs.clear();

    int index = 0;
    for (auto i = m_wallet->m_unspentOutputs.cbegin(); i != m_wallet->m_unspentOutputs.end(); ++i) {
        m_rowsToOutputRefs.insert(std::make_pair(index++, i->first));
    }
}


