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
#ifndef FLOWEE_ACCOUNT_CONFIG_H
#define FLOWEE_ACCOUNT_CONFIG_H

#include <QMap>
#include <QString>
#include <cstdint>

class Wallet;

/**
 * A set of user-level config fields for a wallet.
 * This is owned by the FloweePay application singleton.
 */
class AccountConfig
{
public:
    /// creates an invalid wallet config
    AccountConfig() = default;
    explicit AccountConfig(uint16_t walletId);
    explicit AccountConfig(Wallet *wallet);
    AccountConfig(const AccountConfig &other);

    AccountConfig &operator=(const AccountConfig &other);

    bool isValid() const;

    /**
     * Returns true if the account balance should be counted in the portfolio total balance.
     * Notice that an archived wallet will never be counted in the total portfolio balance.
     */
    bool countBalance() const;
    void setCountBalance(bool newCountBalance);

    bool allowInstaPay() const;
    void setAllowInstaPay(bool on);

    const QMap<QString, int>& fiatInstaPayLimits() const;
    int fiatInstaPayLimit(const QString &currencyCode) const;
    void setFiatInstaPayLimit(const QString &currencyCode, int limitInCent);

    bool isPrivate() const;
    void setIsPrivate(bool newIsPrivate);

private:
    int m_walletId = -1;
};

#endif
