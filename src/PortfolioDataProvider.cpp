/*
 * This file is part of the Flowee project
 * Copyright (C) 2020-2023 Tom Zander <tom@flowee.org>
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
#include "FloweePay.h"
#include "Wallet.h"
#include "WalletConfig.h"

#include <QSettings>

PortfolioDataProvider::PortfolioDataProvider(QObject *parent) : QObject(parent)
{
    auto app = FloweePay::instance();
    for (auto &wallet : app->wallets()) {
        addWalletAccount(wallet);
    }
    selectDefaultWallet();

    connect (app, &FloweePay::walletsChanged, this, [=]() {
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
        selectDefaultWallet();
        emit accountsChanged();
    });
    connect (app, SIGNAL(totalBalanceConfigChanged()), this, SIGNAL(totalBalanceChanged()));
    connect (app, &FloweePay::privateModeChanged, this, [=]() {
        selectDefaultWallet();
        emit accountsChanged();
    });
    connect (this, &PortfolioDataProvider::accountsChanged, [=]() {
        m_isSingleAccount = accounts().length() <= 1;
        emit singleAccountChanged();
    });
    m_isSingleAccount = accounts().length() <= 1;
}

QList<QObject *> PortfolioDataProvider::accounts() const
{
    QList<QObject *> answer;
    const bool privateModeOn = FloweePay::instance()->privateMode();
    for (auto *account : m_accountInfos) {
        if (account->isArchived())
            continue;
        if (!account->userOwnedWallet())
            continue;
        if (privateModeOn && WalletConfig(account->id()).isPrivate())
            continue;
        answer.append(account);
    }
    if (answer.length() == 1) {
        // We have a 'mode' where having exactly one wallet will
        // hide the wallets-list to avoid confusing the user with multi
        // wallet features.
        answer.clear();
    }

    return answer;
}

QList<QObject *> PortfolioDataProvider::rawAccounts() const
{
    QList<QObject *> answer;
    for (auto *account : m_accountInfos) {
        answer.append(account);
    }
    return answer;
}

QList<QObject *> PortfolioDataProvider::archivedAccounts() const
{
    QList<QObject *> answer;
    // we filter out the wallets that are NOT user-owned. Which is essentially the main initial
    // wallet created to allow people to deposit instantly.
    for (auto *account : m_accountInfos) {
        if (account->isArchived() && account->userOwnedWallet())
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
    auto *old = current();
    // when changing, assume that the user noticed the transactions in the account.
    if (old) old->setHasFreshTransactions(false);
    if (item) {
        item->setHasFreshTransactions(false);
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
    const bool privateModeOn = FloweePay::instance()->privateMode();
    PrivacySegment::Priority selectedPriority = PrivacySegment::OnlyManual;
    for (int i = 0; i < m_accounts.size(); ++i) {
        auto wallet = m_accounts.at(i);
        const auto prio = wallet->segment()->priority();
        if (!wallet->userOwnedWallet()) {
            fallback = i;
        } else if (prio < selectedPriority) {
            if (privateModeOn) {
                // downgrade wallets that are 'private' while in private mode.
                WalletConfig config(wallet);
                if (config.isPrivate()) {
                    if (fallback == -1)
                        fallback = i;
                    continue;
                }
            }
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
    const bool privateModeOn = FloweePay::instance()->privateMode();
    for (int i = 0; i < m_accounts.size(); ++i) {
        auto wallet = m_accounts.at(i);
        // skip archived wallet balances.
        if (!wallet->segment() || wallet->segment()->priority() == PrivacySegment::OnlyManual)
            continue;
        WalletConfig config(wallet);
        assert(config.isValid());
        if (!config.countBalance() || (privateModeOn && config.isPrivate()))
            continue;

        rc += wallet->balanceConfirmed();
        rc += wallet->balanceImmature();
        rc += wallet->balanceUnconfirmed();
    }
    return rc;
}

bool PortfolioDataProvider::isSingleAccount() const
{
    return m_isSingleAccount;
}

void PortfolioDataProvider::addWalletAccount(Wallet *wallet)
{
    assert(wallet);
    if (m_accounts.contains(wallet))
        return;
    auto info = new AccountInfo(wallet, this);
    assert(m_accounts.length() == m_accountInfos.length());

    // we store the wallets semi-sorted. Basically just doing an insert-in-place
    // that makes the archived wallets all be at the end and the non-archived ones
    // at the start of the list.
    // Please do note that we don't re-sort later, so this is not a required internal
    // state.
    int index = m_accounts.size();
    if (!info->isArchived()) {
        for (auto i = m_accountInfos.rbegin(); i != m_accountInfos.rend(); ++i) {
            if ((*i)->isArchived())
                --index;
            else
                break;
        }
    }
    m_accounts.insert(index, wallet);
    m_accountInfos.insert(index, info);
    connect (info, SIGNAL(isPrimaryAccountChanged()), this, SLOT(walletChangedPriority()));
    connect (info, SIGNAL(isArchivedChanged()), this, SLOT(walletChangedPriority()));
    connect (info, SIGNAL(balanceChanged()), this, SIGNAL(totalBalanceChanged()));
    connect (info, &AccountInfo::isPrivateChanged, this, [=]() {
        if (FloweePay::instance()->privateMode())
            emit accountsChanged();
    });
    emit accountsChanged();
}

void PortfolioDataProvider::walletChangedPriority()
{
    const AccountInfo * const wallet = qobject_cast<AccountInfo*>(sender());
    if (!wallet)
        return;
    if (wallet->isPrimaryAccount()) {
        // as this just changed, through the UI, we have to mark any other
        // account that was the primary wallet as no longer being one.
        for (auto iter = m_accountInfos.begin(); iter != m_accountInfos.end(); ++iter) {
            if (*iter != wallet && (*iter)->isPrimaryAccount()) {
                (*iter)->setPrimaryAccount(false);
            }
        }
    }
    if (wallet == current() && wallet->isArchived()) {
        selectDefaultWallet();
    }
    emit accountsChanged();
    emit totalBalanceChanged(); // maybe one got (un) archived which changes the total balance
}
