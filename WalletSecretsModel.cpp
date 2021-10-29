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
#include "WalletSecretsModel.h"
#include "Wallet.h"
#include "FloweePay.h"

#include <cashaddr.h>

WalletSecretsModel::WalletSecretsModel(Wallet *wallet, QObject *parent)
    : QAbstractListModel(parent),
      m_wallet(wallet)
{
    assert(m_wallet);
    update();
}

int WalletSecretsModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) // only for the (invalid) root node we return a count, since this is a list not a tree
        return 0;

    return m_selectedPrivates.size();
}

QVariant WalletSecretsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    QMutexLocker locker(&m_wallet->m_lock);
    assert(m_selectedPrivates.size() > index.row());
    auto foundItem = m_wallet->m_walletSecrets.find(m_selectedPrivates.at(index.row()));
    assert(foundItem != m_wallet->m_walletSecrets.end());
    const auto &item = foundItem->second;
    switch (role) {
    case BitcoinAddress: {
        CashAddress::Content c;
        c.type = CashAddress::PUBKEY_TYPE;
        c.hash = std::vector<uint8_t>(item.address.begin(), item.address.end());
        auto s = CashAddress::encodeCashAddr(chainPrefix(), c);
        return QString::fromStdString(s).mid(chainPrefix().size() + 1);
    }
    case PrivateKey:
        return QVariant("PrivKey"); // TODO
    case FromChangeChain:
        return QVariant(item.fromChangeChain);
    case HDIndex:
        return QVariant(item.hdDerivationIndex);
    case Balance:
        return QVariant(static_cast<qint64>(m_wallet->saldoForPrivateKey(foundItem->first)));
    case NumTransactions:
        return QVariant(2); // TODO
    case NumCoins:
        return QVariant(1); // TODO
    }

    return QVariant();
}

QHash<int, QByteArray> WalletSecretsModel::roleNames() const
{
    QHash<int, QByteArray> answer;
    answer[BitcoinAddress] = "address";
    answer[PrivateKey] = "privatekey";
    answer[FromChangeChain] = "isChange";
    answer[HDIndex] = "hdIndex";
    answer[Balance] = "balance";
    answer[NumTransactions] = "numTransactions";
    answer[NumCoins] = "numCoins";

    return answer;
}

bool WalletSecretsModel::showChangeChain() const
{
    return m_showChangeChain;
}

void WalletSecretsModel::setShowChangeChain(bool on)
{
    if (m_showChangeChain == on)
        return;
    m_showChangeChain = on;
    emit showChangeChainChanged();
    update();
}

bool WalletSecretsModel::showUsedAddresses() const
{
    return m_showUsedAddresses;
}

void WalletSecretsModel::setShowUsedAddresses(bool on)
{
    if (m_showUsedAddresses == on)
        return;
    m_showUsedAddresses = on;
    emit showUsedAddressesChanged();
    update();
}

const QString &WalletSecretsModel::searchTerm() const
{
    return m_search;
}

void WalletSecretsModel::search(const QString &newSearch)
{
    if (m_search == newSearch)
        return;
    m_search = newSearch;
    update();
    emit searchChanged();
}

void WalletSecretsModel::update()
{
    assert(m_wallet);
    QMutexLocker locker(&m_wallet->m_lock);
    if (!m_selectedPrivates.isEmpty()) {
        beginRemoveRows(QModelIndex(), 0, m_selectedPrivates.size() - 1);
        m_selectedPrivates.clear();
        endRemoveRows();
    }

    auto iter = m_wallet->m_walletSecrets.begin();
    while (iter != m_wallet->m_walletSecrets.end()) {
        const auto &priv = iter->second;
        bool use = !priv.fromHdWallet || m_showChangeChain == priv.fromChangeChain;

        if (use)
            m_selectedPrivates.append(iter->first);
       ++iter;
    }
    beginInsertRows(QModelIndex(), 0, m_selectedPrivates.size() - 1);
    endInsertRows();
}
