/*
 * This file is part of the Flowee project
 * Copyright (C) 2021-2022 Tom Zander <tom@flowee.org>
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
#include "PriceDataProvider.h"
#include "Logger.h"

#include <QLocale>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringBuilder>

// CoinGecko
static const char *CoinGeckoURL = "https://api.coingecko.com/api/v3/simple/price?ids=bitcoin-cash&vs_currencies=%1";
static const char *CoinGeckoJSONRoot = "bitcoin-cash";
// CoinGecko end

static constexpr int ReloadTimeout = 7 * 60 * 1000;

PriceDataProvider::PriceDataProvider(const QString &countryCode, QObject *parent) : QObject(parent)
{
    if (countryCode.isEmpty())
        setCurrency(QLocale::system());
    else
        setCountry(countryCode);
    QObject::connect(&m_timer, SIGNAL(timeout()), this, SLOT(fetch()));
}

void PriceDataProvider::start()
{
    if (m_priceHistory.get())
        m_priceHistory->initialPopulate();
    m_timer.start(ReloadTimeout);
    fetch();
}

void PriceDataProvider::mock(int price)
{
    m_currentPrice.price = price;
}

void PriceDataProvider::setCurrency(const QLocale &countryLocale)
{
    auto newCurrency = countryLocale.currencySymbol(QLocale::CurrencyIsoCode);
    if (m_currency == newCurrency)
        return;
    m_currency = newCurrency;
    m_currencySymbolPrefix = countryLocale.currencySymbol(QLocale::CurrencySymbol);
    m_currencySymbolPost.clear();
    if (m_currency == QLatin1String("AZN")
            || m_currency == QLatin1String("ILS")
            || m_currency == QLatin1String("IRR")
            || m_currency == QLatin1String("KHR")
            || m_currency == QLatin1String("KZT")
            || m_currency == QLatin1String("PLN")
            || m_currency == QLatin1String("NOK")
            || m_currency == QLatin1String("PYG")
            || m_currency == QLatin1String("RUB")
            || m_currency == QLatin1String("VND")) {
        // these currencies format the name after the numbers part.
        m_currencySymbolPost = QString(" ") + m_currencySymbolPrefix;
        m_currencySymbolPrefix.clear();
    }
    // drop the '.00' behind the prices as this country doesn't traditionlly do that
    m_displayCents = !(m_currency == QLatin1String("JPY")
                   || m_currency == QLatin1String("NOK"));

    emit currencySymbolChanged();
    if (!m_basedir.isEmpty()) {
        assert(m_priceHistory.get());
        // we need to replace the history of our coin to the new history of the new coin.
        loadPriceHistory(m_basedir);

        if (m_timer.isActive()) // implies we are start()-ed
            start();
    }
    emit priceChanged(m_currentPrice.price);
}

void PriceDataProvider::setCountry(const QString &countrycode)
{
    setCurrency(QLocale(countrycode));
}

QString PriceDataProvider::formattedPrice(double amountSats, int price) const
{
    if (price == 0)
        return QString();
    return formattedPrice(priceFor(amountSats, price));
}

int PriceDataProvider::priceFor(double amountSats, int price) const
{
    qint64 fiatValue = amountSats * price;
    fiatValue = (fiatValue + 50000000) / qint64(100000000);
    assert(fiatValue < INT_MAX);
    return static_cast<int>(fiatValue);
}

QString PriceDataProvider::formattedPrice(int fiatValue) const
{
    // convert cheaply (low number of mallocs) to a price.
    // since our fiat is in cents, we assume we may add up to two leading zeros.
    static const QString priceTemplate("00%1");
    QString centsPrice = priceTemplate.arg(std::abs(fiatValue));
    const int offset = std::min<int>(2, centsPrice.length() - 3); // number of those zeros to cut off again.
    const int length = centsPrice.length() - 2 - offset;

    QString actualPrice = centsPrice.mid(offset, length);

    QString dummy; // used when we inserted thousand separators. QStringRef needs the thing it refers to to outlive it.
    // group size is 3
    if (length > 3 && length <= 15) {
        // lets insert some thousands separators.
        char buf[21];
        memset(buf, 0, sizeof(buf));
        int add = 0;
        for (int i = 0; i < actualPrice.length(); ++i) {
            // insert thousands separators.
            if (i > 0 && actualPrice.length() % 3  == i % 3)
                buf[i + add++] = QLocale::system().groupSeparator().at(0).toLatin1();

            assert(actualPrice.at(i).unicode() < 127); // only digits
            buf[i + add] = actualPrice.at(i).unicode();
        }
        dummy = QString::fromLatin1(buf);
        actualPrice = dummy.left(dummy.size() - (m_displayCents ? 0 : 2));
    }

    if (m_displayCents) {
        return m_currencySymbolPrefix
                % (fiatValue < 0 ? QLatin1String("-") : QLatin1String(""))
                % actualPrice
                % QLocale::system().decimalPoint()
                % centsPrice.right(2)
                % m_currencySymbolPost;
    }
    else {
        return  m_currencySymbolPrefix
                % (fiatValue < 0 ? QLatin1String("-") : QLatin1String(""))
                % actualPrice
                % m_currencySymbolPost;
    }
}

int PriceDataProvider::historicalPrice(const QDateTime &timestamp) const
{
    if (m_priceHistory.get() == nullptr)
        return m_currentPrice.price;

    return m_priceHistory->historicalPrice(timestamp.toSecsSinceEpoch());
}

QString PriceDataProvider::priceToStringSimple(int cents) const
{
    auto value = QString::number(cents);
    if (m_displayCents) {
        const QChar comma = QLocale::system().decimalPoint().at(0);
        if (cents < 10)
            value = "0" + value;
        if (cents < 100)
            value = "0" % comma % value;
        else
            value = value.left(value.size() - 2) % comma % value.right(2);
    }
    return value;
}

void PriceDataProvider::fetch()
{
    QString url(CoinGeckoURL);
    QNetworkRequest req(QUrl(url.arg(m_currency.toLower())));
    logInfo() << "fetch" << m_currency;
    m_reply = m_network.get(req);
    connect(m_reply, SIGNAL(finished()), this, SLOT(finishedDownload()));
}

void PriceDataProvider::finishedDownload()
{
    logInfo() << "finishDownload";
    if (m_reply == nullptr)
        return;
    const auto data = m_reply->readAll();
    const bool failed = m_reply->error() != QNetworkReply::NoError || data.isEmpty();
    if (failed)
        logCritical() << "   failed";
    m_reply->deleteLater();
    m_reply = nullptr;
    if (failed) {
        m_timer.stop();
        if (m_failedCount++ < 5) {
            // things like a DNS caching server or a flaky wifi can add a lot
            // of delays between a fetch() and a failed reply coming back.
            // So we take a middle road that works for all cases, the first 5
            // times we instantly check again. In the case of networking delays, those
            // will be our waiting time.
            // otherwise, should it return instantly, we just cycle through those 5 in
            // a second and then we poll every 20 seconds below.
            fetch();
        } else {
            m_timer.start(20 * 1000);
        }
        return;
    }
    try {
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isEmpty())
            throw std::runtime_error("Failed parsing the data");

        auto root = doc.object();
        // coingecko
        auto section = root.value(CoinGeckoJSONRoot).toObject();
        auto price = section.value(m_currency.toLower());
        if (price.isUndefined()) { // our provider does not support this coin.
            logCritical() << " provider does not support this coin" << m_currency;
            setCountry("en_US");
            return;
        }
        m_currentPrice.price = price.toDouble() * 100;
        m_currentPrice.timestamp = time(nullptr);
        emit priceChanged(m_currentPrice.price);
    } catch (const std::runtime_error &error) {
        logWarning() << "PriceDataProvider failed." << error.what();
        if (!data.isEmpty())
            logInfo() << QString::fromUtf8(data);
        m_timer.start(20 * 1000);
        return;
    }
    logCritical() << "Current fiat price: " << m_currencySymbolPrefix << m_currentPrice.price << m_currencySymbolPost;
    m_timer.start(ReloadTimeout);
}

QString PriceDataProvider::currencySymbolPost() const
{
    return m_currencySymbolPost;
}

void PriceDataProvider::loadPriceHistory(const QString &basedir)
{
    m_basedir = basedir;
    m_priceHistory.reset(new PriceHistoryDataProvider(basedir, m_currency));
    // take the last known price from our historical module to have something
    // mostly useful until we manage to fetch the data from the life feeds.
    auto lastKnownPrice = historicalPrice(QDateTime::currentDateTimeUtc());
    if (lastKnownPrice == 0)
        lastKnownPrice = 10000; // if we never fetched, set to 100,-
    m_currentPrice.price = lastKnownPrice;
    connect (this, &PriceDataProvider::priceChanged,
             m_priceHistory.get(), [=](int price) {
        m_priceHistory->addPrice(currencyName(), QDateTime::currentSecsSinceEpoch(), price);
    });
}

QString PriceDataProvider::currencySymbolPrefix() const
{
    return m_currencySymbolPrefix;
}

bool PriceDataProvider::displayCents() const
{
    return m_displayCents;
}
