/*
 * This file is part of the Flowee project
 * Copyright (C) 2020 Tom Zander <tomz@freedommail.ch>
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
    connect (FloweePay::instance(), &FloweePay::walletsChanged, [=]() {
        for (auto w : FloweePay::instance()->wallets()) {
            if (!m_accounts.contains(w)) {
                addWalletAccount(w);
            }
        }
    });
}

QList<AccountInfo *> PortfolioDataProvider::accounts() const
{
    return m_accountInfos;
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
    for (int i = 0; i < m_accounts.size(); ++i) {
        auto wallet = m_accounts.at(i);
        if (wallet->segment()->priority() == PrivacySegment::First) {
            m_currentAccount = i;
            emit currentChanged();
            break;
        }
    }
}

void PortfolioDataProvider::addWalletAccount(Wallet *wallet)
{
    if (m_accounts.contains(wallet))
        return;
    m_accounts.append(wallet);
    auto info = new AccountInfo(wallet, this);
    m_accountInfos.append(info);
    connect (info, SIGNAL(isDefaultWalletChanged()), this, SLOT(walletChangedPriority()));
    accountsChanged();
}

void PortfolioDataProvider::walletChangedPriority()
{
    AccountInfo *wallet = qobject_cast<AccountInfo*>(sender());
    if (!wallet)
        return;
    if (wallet->isDefaultWallet()) {
        // as this just changed, through the UI, we have to mark any other
        // wallet that was a default wallet as no longer being one.
        for (auto info : m_accountInfos) {
            if (info != wallet && info->isDefaultWallet())
                info->setDefaultWallet(false);
        }
    }
}
