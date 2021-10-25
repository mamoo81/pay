/*
 * This file is part of the Flowee project
 * Copyright (C) 2020 Tom Zander <tom@flowee.org>
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
        for (auto &w : FloweePay::instance()->wallets()) {
            if (!m_accounts.contains(w)) {
                addWalletAccount(w);
            }
        }
    });
}

QList<AccountInfo *> PortfolioDataProvider::accounts() const
{
    QList<AccountInfo *> answer;
    // we filter out the wallets that are NOT user-owned. Which is essentially the main initial
    // wallet created to allow people to deposit instantly.
    for (auto *account : m_accountInfos) {
        if (account->userOwnedWallet())
            answer.append(account);
    }
    // if the only wallet(s) are not user owned, share those with the GUI.
    if (answer.isEmpty() && !m_accountInfos.isEmpty())
        return m_accountInfos;
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

Payment *PortfolioDataProvider::createPaymentObject(const QString &address, double value)
{
    auto p = new Payment(m_accounts.at(m_currentAccount), value);
    p->setPreferSchnorr(FloweePay::instance()->preferSchnorr());

    try {
        p->setTargetAddress(address);
        return p;
    } catch (...) {
        logFatal() << "Got a non-validating address, can't start payment";
        delete p;
        return nullptr;
    }

}

QObject *PortfolioDataProvider::startPayToAddress(const QString &address, BitcoinValue *bitcoinValue)
{
    assert(bitcoinValue);
    if (m_currentAccount == -1 || bitcoinValue == nullptr)
        return nullptr;
    return createPaymentObject(address, bitcoinValue->value());
}

QObject *PortfolioDataProvider::startPayAllToAddress(const QString &address)
{
    // -1 is defined by the Payment::setPaymentAmount() to be the 'send all' indicator.
    return createPaymentObject(address, -1);
}

void PortfolioDataProvider::selectDefaultWallet()
{
    int fallback = -1;
    for (int i = 0; i < m_accounts.size(); ++i) {
        auto wallet = m_accounts.at(i);
        if (wallet->segment()->priority() == PrivacySegment::First) {
            m_currentAccount = i;
            emit currentChanged();
            break;
        }
        if (!wallet->userOwnedWallet()) {
            fallback = i;
        }
    }
    // when the app went and made an empty wallet, select that so we have at least something selected.
    if (m_currentAccount == -1) {
        if (fallback >= 0)
            m_currentAccount = fallback;
        else if (!m_accounts.isEmpty())
            m_currentAccount = 0;;
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
}
