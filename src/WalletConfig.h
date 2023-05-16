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
#ifndef FLOWEE_WALLET_CONFIG_H
#define FLOWEE_WALLET_CONFIG_H

#include <QMap>
#include <QString>
#include <cstdint>

class Wallet;

/**
 * A set of user-level config fields for a wallet.
 * This is owned by the FloweePay application singleton.
 */
class WalletConfig
{
public:
    /// creates an invalid wallet config
    WalletConfig() = default;
    explicit WalletConfig(uint16_t walletId);
    explicit WalletConfig(Wallet *wallet);
    WalletConfig(const WalletConfig &other);

    WalletConfig &operator=(const WalletConfig &other);

    bool isValid() const;


    bool countBalance() const;
    void setCountBalance(bool newCountBalance);

    bool allowInstaPay() const;
    void setAllowInstaPay(bool on);

    const QMap<QString, int>& fiatInstaPayLimits() const;
    int fiatInstaPayLimit(const QString &currencyCode) const;
    void setFiatInstaPayLimit(const QString &currencyCode, int limitInCent);

private:
    int m_walletId = -1;
};

#endif
