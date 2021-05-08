/*
 * This file is part of the Flowee project
 * Copyright (C) 2021 Tom Zander <tom@flowee.org>
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
#ifndef PRICEDATAPROVIDER_H
#define PRICEDATAPROVIDER_H

#include <QNetworkAccessManager>
#include <QObject>
#include <QTimer>

class PriceDataProvider : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int price READ price NOTIFY priceChanged)
    Q_PROPERTY(QString currencyName READ currencyName NOTIFY currencySymbolChanged)
public:
    explicit PriceDataProvider(QObject *parent = nullptr);

    void start();

    QString currencyName() const {
        return m_currency;
    }

    // current price in cents
    int price() const {
        return m_currentPrice.price;
    }

    /**
     * Return a formatted string with the locale-defined price of the amount of \a sats.
     * This takes the BCH amount (in sats) and renders it into a price as the current locale defines,
     * based on \a price.
     */
    Q_INVOKABLE QString formattedPrice(double amountSats, int price) const;

signals:
    void priceChanged();
    void currencySymbolChanged();

private slots:
    void fetch();
    void finishedDownload();

private:
    struct Price {
        int32_t price = 0;
        uint32_t timestamp = 0;
    };

    Price m_currentPrice;
    QNetworkAccessManager m_network;
    QNetworkReply *m_reply = nullptr;
    QString m_currency;
    QString m_currencySymbolPrefix, m_currencySymbolPost;
    QTimer m_timer;
};

#endif
