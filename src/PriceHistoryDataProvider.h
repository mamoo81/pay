/*
 * This file is part of the Flowee project
 * Copyright (C) 2022-2023 Tom Zander <tom@flowee.org>
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

#include <QObject>
#include <streaming/ConstBuffer.h>

#include <QNetworkAccessManager>

#include <boost/filesystem.hpp>

class QFile;

class PriceHistoryDataProvider : public QObject
{
    Q_OBJECT
public:
    explicit PriceHistoryDataProvider(const QString &basedir, const QString &currency, QObject *parent = nullptr);

    void addPrice(const QString &currency, uint32_t timestamp, int price);

    /// Used in historicalPrice()
    enum HistoricalPriceAccuracy {
        Accurate, ///< Return zero if we don't have historical data of that day
        Nearest ///< Return the nearest known price.
    };

    int historicalPrice(uint32_t timestamp, HistoricalPriceAccuracy = Nearest) const;

    QString currencyName() const;

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
        bool hasBlob = false;
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
