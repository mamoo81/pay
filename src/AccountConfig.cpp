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

#include "AccountConfig.h"
#include "FloweePay.h"
#include "Wallet.h"

AccountConfig::AccountConfig(const AccountConfig &other)
    : m_walletId(other.m_walletId)
{
}

AccountConfig::AccountConfig(uint16_t walletId)
    : m_walletId(walletId)
{
}

AccountConfig::AccountConfig(Wallet *wallet)
    : AccountConfig(wallet->segment()->segmentId())
{
}

AccountConfig &AccountConfig::operator=(const AccountConfig &other)
{
    m_walletId = other.m_walletId;
    return *this;
}

bool AccountConfig::isValid() const
{
    if (m_walletId == -1)
        return false;

    auto configs = FloweePay::instance()->m_accountConfigs;
    auto i = configs.find(m_walletId);
    if (i == configs.end())
        return false;
    return true;
}

bool AccountConfig::countBalance() const
{
    auto configs = FloweePay::instance()->m_accountConfigs;
    auto i = configs.find(m_walletId);
    assert(i != configs.end());
    return i->countBalance;
}

void AccountConfig::setCountBalance(bool newCountBalance)
{
    auto *fp = FloweePay::instance();
    auto configs = fp->m_accountConfigs;
    auto i = configs.find(m_walletId);
    assert(i != configs.end());
    if (i->countBalance == newCountBalance)
        return;
    i->countBalance = newCountBalance;
    fp->m_accountConfigs = configs;
    fp->startSaveData_priv();
    emit fp->totalBalanceConfigChanged();
}

bool AccountConfig::allowInstaPay() const
{
    auto configs = FloweePay::instance()->m_accountConfigs;
    auto i = configs.find(m_walletId);
    assert(i != configs.end());
    return i->allowInstaPay;
}

void AccountConfig::setAllowInstaPay(bool on)
{
    auto *fp = FloweePay::instance();
    auto configs = fp->m_accountConfigs;
    auto i = configs.find(m_walletId);
    assert(i != configs.end());
    if (i->allowInstaPay == on)
        return;
    i->allowInstaPay = on;
    fp->m_accountConfigs = configs;
    fp->startSaveData_priv();
}

const QMap<QString, int> &AccountConfig::fiatInstaPayLimits() const
{
    auto configs = FloweePay::instance()->m_accountConfigs;
    auto i = configs.find(m_walletId);
    assert(i != configs.end());
    return i->fiatInstaPayLimits;
}

int AccountConfig::fiatInstaPayLimit(const QString &currencyCode) const
{
    auto configs = FloweePay::instance()->m_accountConfigs;
    auto i = configs.find(m_walletId);
    assert(i != configs.end());
    return i->fiatInstaPayLimits.value(currencyCode);
}

void AccountConfig::setFiatInstaPayLimit(const QString &currencyCode, int limitInCent)
{
    auto *fp = FloweePay::instance();
    auto configs = fp->m_accountConfigs;
    auto i = configs.find(m_walletId);
    assert(i != configs.end());
    if (i->fiatInstaPayLimits.value(currencyCode) == limitInCent)
        return;
    i->fiatInstaPayLimits[currencyCode] = limitInCent;
    fp->m_accountConfigs = configs;
    fp->startSaveData_priv();
}

bool AccountConfig::isPrivate() const
{
    auto configs = FloweePay::instance()->m_accountConfigs;
    auto i = configs.find(m_walletId);
    assert(i != configs.end());
    return i->privateWallet;
}

void AccountConfig::setIsPrivate(bool newIsPrivate)
{
    auto *fp = FloweePay::instance();
    auto configs = fp->m_accountConfigs;
    auto i = configs.find(m_walletId);
    assert(i != configs.end());
    if (i->privateWallet == newIsPrivate)
        return;
    i->privateWallet = newIsPrivate;
    fp->m_accountConfigs = configs;
    fp->startSaveData_priv();
}
