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
#ifndef PRICEHISTORYDATAPROVIDER_H
#define PRICEHISTORYDATAPROVIDER_H

#include <QDateTime>
#include <QFile>
#include <QNetworkAccessManager>
#include <QObject>

#include <streaming/ConstBuffer.h>

#include <boost/filesystem.hpp>

class PriceHistoryDataProvider : public QObject
{
    Q_OBJECT
public:
    explicit PriceHistoryDataProvider(const QString &basedir, const QString &currency, QObject *parent = nullptr);

    void addPrice(const QString &currency, uint32_t timestamp, int price);

    Q_INVOKABLE int historicalPrice(const QDateTime &timestamp) const {
        return historicalPrice(timestamp.toSecsSinceEpoch());
    }
    int historicalPrice(uint32_t timestamp) const;

    QString currencyName() const;
    void setCurrency(const QString &newCurrency);

    /**
     * Adding values may trigger the 'processLog' method
     * automatically unless this value is set to false.
     * Default is true.
     */
    void setAllowLogCompression(bool newAllowLogCompression);
    bool allowLogCompression() const;

    /// if there is no historical data, try to populate it
    void initialPopulate();

public slots:
    void processLog();

private:
    QString m_currency;

    struct Currency {
        // a single currency and historical things we know about it.
        QString id; ///< the language ID (EUR/USD etc)
        Streaming::ConstBuffer valueBlob; // a raw list of time/value pairs.

        /// a append-only list of time/value pairs (as stored in the log file)
        std::vector<std::pair<uint32_t, int> > logValues;
        /// the log file where we append newly queries currency values
        QFile *log = nullptr;
    };

    const Currency *currencyData(const QString &name) const;
    enum AutoCreate {
        FetchOnly,
        FetchOrCreate
    };
    Currency *currencyData(const QString &name, AutoCreate autoCreate);

    std::vector<Currency> m_currencies;
    QString m_basedir;
    bool m_allowLogCompression = true;
};

class InitialHistoryFetcher : public QObject
{
    Q_OBJECT
public:
    explicit InitialHistoryFetcher(QObject *parent = nullptr);

    void fetch(const QString &path, const QString &currency);

signals:
    void success(const QString &currency);

private:
    QNetworkAccessManager m_network;
};

#endif
