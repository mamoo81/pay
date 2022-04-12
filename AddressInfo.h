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
#ifndef ADDRESSINFO_H
#define ADDRESSINFO_H

#include <QObject>

/**
 * Information about an address owned by Flowee Pay.
 */
class AddressInfo : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString address READ address CONSTANT)
    Q_PROPERTY(QString accountName READ accountName CONSTANT)
    Q_PROPERTY(int coins READ coins CONSTANT)
    Q_PROPERTY(int historicalCoins READ historicalCoins CONSTANT)
    Q_PROPERTY(double saldo READ saldo CONSTANT)
    Q_PROPERTY(int accountId READ accountId CONSTANT)
public:
    explicit AddressInfo(const QString &address, QObject *parent = nullptr);

    const QString &address() const;

    const QString &accountName() const;
    void setAccountName(const QString &name);

    double saldo() const;
    void setSaldo(double newSaldo);

    int coins() const;
    void setCoins(int newCoins);

    int historicalCoins() const;
    void setHistoricalCoins(int newHistoricalCoins);

    int accountId() const;
    void setAccountId(int id);

private:
    QString m_address;
    QString m_accountName;
    double m_saldo = -1;
    int m_coins = -1;
    int m_historicalCoins = -1;
    int m_accountId = -1;
};

#endif
