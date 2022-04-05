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
#include "PortfolioDataProvider.h"
#include "AccountInfo.h"
#include "BitcoinValue.h"
#include "FloweePay.h"
#include "Payment.h"
#include "Wallet.h"

#include <QSettings>

PortfolioDataProvider::PortfolioDataProvider(QObject *parent) : QObject(parent)
{
    connect (FloweePay::instance(), &FloweePay::walletsChanged, this, [=]() {
        const auto &wallets = FloweePay::instance()->wallets();
        if (wallets.isEmpty()) {
            m_accounts.clear();
            for (auto ai : m_accountInfos) {
                ai->deleteLater();
            }
            m_accountInfos.clear();
        }
        for (auto &w : wallets) {
            addWalletAccount(w);
        }
        emit accountsChanged(); // not strictly needed, but safe to have.
    });
}

QList<QObject *> PortfolioDataProvider::accounts() const
{
    QList<QObject *> answer;
    // we filter out the wallets that are NOT user-owned. Which is essentially the main initial
    // wallet created to allow people to deposit instantly.
    for (auto *account : m_accountInfos) {
        if (account->userOwnedWallet() && !account->isArchived())
            answer.append(account);
    }
    // if the only wallet(s) are not user owned, share those with the GUI.
    if (answer.isEmpty() && !m_accountInfos.isEmpty()) {
        for (auto *account : m_accountInfos) {
            if (account->isArchived())
                answer.append(account);
        }
    }
    return answer;
}

QList<QObject *> PortfolioDataProvider::archivedAccounts() const
{
    QList<QObject *> answer;
    // we filter out the wallets that are NOT user-owned. Which is essentially the main initial
    // wallet created to allow people to deposit instantly.
    for (auto *account : m_accountInfos) {
        if (account->isArchived())
            answer.append(account);
    }
    return answer;
}

AccountInfo *PortfolioDataProvider::current() const
{
    if (m_currentAccount >= 0 && m_currentAccount < m_accountInfos.size())
        return m_accountInfos.at(m_currentAccount);
    return nullptr;
}

void PortfolioDataProvider::setCurrent(AccountInfo *item)
{
    if (item) {
        int index = m_accountInfos.indexOf(item);
        if (index == m_currentAccount)
            return;
        m_currentAccount = index;
    }
    else {
        m_currentAccount = -1;
    }
    emit currentChanged();
}

void PortfolioDataProvider::selectDefaultWallet()
{
    int fallback = -1;
    int current = -1;
    PrivacySegment::Priority selectedPriority = PrivacySegment::OnlyManual;
    for (int i = 0; i < m_accounts.size(); ++i) {
        auto wallet = m_accounts.at(i);
        const auto prio = wallet->segment()->priority();
        if (!wallet->userOwnedWallet()) {
            fallback = i;
        } else if (prio < selectedPriority) {
            current = i;
            selectedPriority = prio;
        }
    }
    // when the app went and made an empty wallet, select that so we have at least something selected.
    if (current == -1)
        current = fallback;

    if (current != m_currentAccount) { // changed?
        m_currentAccount = current;
        emit currentChanged();
    }
}

double PortfolioDataProvider::totalBalance() const
{
    double rc = 0;
    for (int i = 0; i < m_accounts.size(); ++i) {
        auto wallet = m_accounts.at(i);
        rc += wallet->balanceConfirmed();
        rc += wallet->balanceImmature();
        rc += wallet->balanceUnconfirmed();
    }
    return rc;
}

void PortfolioDataProvider::addWalletAccount(Wallet *wallet)
{
    if (m_accounts.contains(wallet))
        return;
    m_accounts.append(wallet);
    auto info = new AccountInfo(wallet, this);
    m_accountInfos.append(info);
    connect (info, SIGNAL(isDefaultWalletChanged()), this, SLOT(walletChangedPriority()));
    connect (info, SIGNAL(isArchivedChanged()), this, SLOT(walletChangedPriority()));
    connect (info, SIGNAL(balanceChanged()), this, SIGNAL(totalBalanceChanged()));
    emit accountsChanged();
}

void PortfolioDataProvider::walletChangedPriority()
{
    AccountInfo *wallet = qobject_cast<AccountInfo*>(sender());
    if (!wallet)
        return;
    if (wallet->isDefaultWallet()) {
        // as this just changed, through the UI, we have to mark any other
        // wallet that was a default wallet as no longer being one.
        for (auto &info : m_accountInfos) {
            if (info != wallet && info->isDefaultWallet())
                info->setDefaultWallet(false);
        }
    }
    emit accountsChanged();
}
