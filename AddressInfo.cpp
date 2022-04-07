/*
 * This file is part of the Flowee project
 * Copyright (C) 2022 Tom Zander <tom@flowee.org>
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
#include "AddressInfo.h"
#include "AccountInfo.h"

AddressInfo::AddressInfo(const QString &address, QObject *parent)
    : QObject{parent},
      m_address(address)
{
}

const QString &AddressInfo::address() const
{
    return m_address;
}

const QString &AddressInfo::walletName() const
{
    return m_walletName;
}

void AddressInfo::setWalletName(const QString &newWalletName)
{
    m_walletName = newWalletName;
}

double AddressInfo::saldo() const
{
    return m_saldo;
}

void AddressInfo::setSaldo(double newSaldo)
{
    m_saldo = newSaldo;
}

int AddressInfo::coins() const
{
    return m_coins;
}

void AddressInfo::setCoins(int newCoins)
{
    m_coins = newCoins;
}

int AddressInfo::historicalCoins() const
{
    return m_historicalCoins;
}

void AddressInfo::setHistoricalCoins(int newHistoricalCoins)
{
    m_historicalCoins = newHistoricalCoins;
}

int AddressInfo::accountId() const
{
    return m_accountId;
}

void AddressInfo::setAccountId(int id)
{
    m_accountId = id;
}
