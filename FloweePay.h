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
#ifndef FLOWEEPAY_H
#define FLOWEEPAY_H

#include <QObject>

#include <DownloadManager.h>
#include <WorkerThreads.h>

#include <QList>
#include <QString>

class Wallet;

class FloweePay : public QObject, WorkerThreads
{
    Q_OBJECT
    Q_PROPERTY(QString unitName READ unitName NOTIFY unitNameChanged)
    Q_ENUMS(UnitsOfBitcoin StringType)
public:
    enum StringType {
        Unknown = 0,
        PrivateKey,
        CashPKH,
        CashSH,
        LegacyPKH,
        LegacySH
    };
    enum UnitOfBitcoin {
        BCH,
        MilliBCH,
        MicroBCH,
        Bits
    };
    FloweePay();
    ~FloweePay();

    static FloweePay *instance();

    QList<Wallet *> wallets() const;

    DownloadManager* p2pNet();

    /// return the app data location
    QString basedir() const;

    /// for a price, in satoshis, return a formatted string in unitName().
    Q_INVOKABLE QString priceToString(qint64 price) const;

    /// create a new wallet with an optional name.
    Q_INVOKABLE void createNewWallet(const QString &walletName = QString());
    /// swipe a paper wallet with an optional name
    Q_INVOKABLE void createImportedWallet(const QString &privateKey, const QString &walletName);

    /// take a bitcoin-address and identify the type.
    Q_INVOKABLE StringType identifyString(const QString &string) const;

    /// returns the unit of our prices. BCH, for instance.
    QString unitName() const;

signals:
    void loadComplete();
    /// \internal
    void loadComplete_priv();

    void unitNameChanged();
    void walletsChanged();

private:
    void init();
    void saveData();
    void saveAll();
    Wallet *createWallet(const QString &name);

    UnitOfBitcoin m_unit = BCH;
    QString m_basedir;
    std::unique_ptr<DownloadManager> m_downloadManager;
    QList<Wallet*> m_wallets;
};

#endif
