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
    Q_PROPERTY(bool displayCents READ displayCents CONSTANT)
public:
    explicit PriceDataProvider(QObject *parent = nullptr);

    void start();
    void mock(int price);

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

    /**
     * Return a formatted string with the locale-defined price of a fiat price from \a cents.
     */
    Q_INVOKABLE QString formattedPrice(int cents) const;

    /**
     * Returns if the locale defined currency wants to display cents.
     *
     * Note: The typical 2 digits behind the separator are lazily called 'cents' here, even
     * though the actual naming is differnt in different currencies.
     */
    bool displayCents() const;

signals:
    void priceChanged(int32_t price);
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
    bool m_displayCents = true; // if true, display 2 digits behind the unit-separator
    int m_failedCount = 0;
    QTimer m_timer;
};

#endif
