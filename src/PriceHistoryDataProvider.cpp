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
#include "PriceHistoryDataProvider.h"
#include "streaming/BufferPools.h"

#include <QDirIterator>
#include <QCoreApplication>
#include <QTimer>
#include <QNetworkReply>

PriceHistoryDataProvider::PriceHistoryDataProvider(const QString &basedir, const QString &currency_, QObject *parent)
    : QObject{parent},
      m_currency(currency_),
      m_basedir(basedir + "/fiat")
{
    if (!QDir("/").mkpath(m_basedir)) // in case it didn't exist, it should now
        logFatal() << "Failed to create basedir" << m_basedir;

    QDirIterator iter(m_basedir, QDir::Files);
    while (iter.hasNext()) {
        iter.next();
        QString currency = iter.fileName();
        const bool isLog = currency.endsWith(".LOG");
        if (isLog) // cut off the extension
            currency = currency.left(currency.size() - 4);
        const qint64 fileSize = iter.fileInfo().size();
        if (!isLog && fileSize > 50000) {
            logInfo() << "Big file in fiat dir, skipping:" << iter.fileName();
            continue;
        }
        Currency *data = currencyData(currency, FetchOrCreate);
        QFile input(iter.filePath());
        if (!input.open(QIODevice::ReadOnly)) {
            logCritical() << "Failed to read fiat file" << iter.filePath();
            continue;
        }
        if (isLog) {
            char buf[8];
            while (input.read(buf, 8) > 0) {
                data->logValues.push_back(std::make_pair(
                    le32toh(*reinterpret_cast<uint32_t*>(buf)),
                    le32toh(*reinterpret_cast<int*>(buf + 4))));
            }
        } else {
            auto &pool = Streaming::pool(static_cast<int>(fileSize));
            input.read(pool.begin(), fileSize);
            data->valueBlob = pool.commit(fileSize);
            data->hasBlob = true;
        }
    }
}

void PriceHistoryDataProvider::addPrice(const QString &currency, uint32_t timestamp, int price)
{
    assert(price >= 0);
    // we add the price always to the LOG file (both the in-memory as well as the disk one)
    Currency *data = currencyData(currency, FetchOrCreate);
    assert(data);
    assert(data->log);
    data->logValues.push_back(std::make_pair(timestamp, price));
    char buf[8];
    *reinterpret_cast<uint32_t*>(buf) = htole32(timestamp);
    *reinterpret_cast<uint32_t*>(buf + 4) = htole32(price);
    data->log->write(buf, 8);
    data->log->flush();

    if (m_allowLogCompression
            && data->logValues.size() > 30 // check for people that rarely start the app
            && timestamp - data->logValues.front().first > 3 * 30 * 24 * 60 * 60) {
        // If we accumulated 3 months of log values,
        // process the log and generate an averaged lookup.
        QTimer::singleShot(400, this, SLOT(processLog()));
    }
}

int PriceHistoryDataProvider::historicalPrice(uint32_t timestamp) const
{
    const Currency *data = currencyData(m_currency);
    int answer = 0;
    if (!data)
        return answer;
    uint32_t prevTimestamp = 0;

    // the log is per definition about newer values than the blob.
    if (!data->logValues.empty()) {
        prevTimestamp = data->logValues.front().first;
        answer = data->logValues.front().second;

        for (auto i = data->logValues.begin(); i != data->logValues.end(); ++i) {
            if (i->first >= timestamp) {
                const int diff1 = timestamp - prevTimestamp;
                const int diff2 = i->first - timestamp;
                if (diff1 > diff2) // closest one is 'i'
                    answer = i->second;
                break;
            }
            answer = i->second;
            prevTimestamp = i->first;
        }
        if (timestamp >= prevTimestamp)
            return answer;
    }
    // if we have a blob, check if we can improve our answer
    // with a closer timestamp
    if (!data->valueBlob.isEmpty()) {
        const uint32_t *blob = reinterpret_cast<const uint32_t*>(data->valueBlob.begin());
        const int count = data->valueBlob.size() / 4;

        for (int i = 0; i < count; i += 2) {
            const auto time = le32toh(blob[i]);
            const auto value = le32toh(blob[i + 1]);

            if (time >= timestamp) {
                const int diff1 = timestamp - prevTimestamp;
                const int diff2 = time - timestamp;
                if (diff1 > diff2) // closest one is 'i'
                    answer = value;
                break;
            }
            answer = value;
            prevTimestamp = time;
        }
        // if (timestamp >= prevTimestamp)
            return answer;
    }
    // TODO maybe check if the value is between the blob and the log

    return answer;
}

QString PriceHistoryDataProvider::currencyName() const
{
    return m_currency;
}

namespace {
struct Day {
    Day() { reset(); }

    void add(const std::pair<uint32_t, int> &measurement) {
        if (count++ == 0)
            firstTimeStamp = measurement.first;
        timestampCum += measurement.first;
        valueCum += measurement.second;
    }
    bool isNewDay(uint32_t time) const {
        if (count == 0)
            return false;
        auto diff = time - firstTimeStamp;
        return diff >= 60 * 60 * 24;
    }

    void writeAverage(Streaming::BufferPool &pool)
    {
        if (count > 0) {
            char buf[8];
            *reinterpret_cast<uint32_t*>(buf) = htole32(timestampCum / count);
            *reinterpret_cast<uint32_t*>(buf + 4) = htole32(valueCum / count);
            pool.write(buf, 8);
        }
    }

    void reset() {
        count = 0;
        firstTimeStamp = 0;
        timestampCum = 0;
        valueCum = 0;
    }

    int count;
    uint32_t firstTimeStamp;
    uint64_t timestampCum;
    uint64_t valueCum;
};
}


void PriceHistoryDataProvider::processLog()
{
    Currency *data = currencyData(m_currency, FetchOnly);
    if (!data)
        return;

    assert(data->log);
    data->log->close();

    auto &pool = Streaming::pool(data->logValues.size() * 8 + data->valueBlob.size());
    pool.write(data->valueBlob);

    // Iterate over the log and for each 24h period compress all items into
    // one entry for that day.
    Day day;
    int days = 0;
    for (auto i = data->logValues.begin(); i != data->logValues.end(); ++i) {
        if (i->second > 0) {
            if (day.isNewDay(i->first)) {
                ++days;
                day.writeAverage(pool);
                day.reset();
            }
            day.add(*i);
        }
    }
    day.writeAverage(pool);
    ++days;
    logInfo() << "Extracted" << days << "days from LOG";

    QString fiatPath("%1/%2");
    fiatPath = fiatPath.arg(m_basedir, m_currency);
    QFile blobData(fiatPath);
    if (!blobData.open(QIODevice::WriteOnly))
        throw std::runtime_error("PriceHistory: Failed to open file for write");
    data->valueBlob = pool.commit();
    auto count = blobData.write(data->valueBlob.begin(), data->valueBlob.size());
    assert(count == data->valueBlob.size());
    data->logValues.clear();
    data->hasBlob = true;

    data->log->open(QIODevice::WriteOnly | QIODevice::Truncate);
}

const PriceHistoryDataProvider::Currency *PriceHistoryDataProvider::currencyData(const QString &name) const
{
    for (auto i = m_currencies.begin(); i != m_currencies.end(); ++i) {
        if (i->id == name)
            return &*i;
    }
    return nullptr;
}

PriceHistoryDataProvider::Currency *PriceHistoryDataProvider::currencyData(const QString &name, AutoCreate autoCreate)
{
    auto answer = currencyData(name);
    if (!answer && autoCreate == FetchOrCreate) {
        QString logPath("%1/%2.LOG");
        logPath = logPath.arg(m_basedir, name);
        Currency c;
        c.id = name;
        c.log = new QFile(logPath, this);
        if (!c.log->open(QIODevice::WriteOnly | QIODevice::Append))
            throw std::runtime_error("PriceHistory: Failed to open LOG file for write");
        m_currencies.push_back(c);
        return &m_currencies.back();
    }
    return const_cast<Currency*>(answer);
}

bool PriceHistoryDataProvider::allowLogCompression() const
{
    return m_allowLogCompression;
}

void PriceHistoryDataProvider::initialPopulate()
{
    auto cur = currencyData(m_currency, FetchOnly);
    if (cur && !cur->hasBlob) {
        logCritical() << "populate!";
        cur->hasBlob = true; // avoid starting fetcher again
        InitialHistoryFetcher *f = new InitialHistoryFetcher(this);
        connect (f, &InitialHistoryFetcher::success, f, [=](const QString &currency) {
            // load this file into a currency object.
            Currency *data = currencyData(currency, FetchOrCreate);
            QFile input(m_basedir + '/' + currency);
            if (input.open(QIODevice::ReadOnly)) {
                const auto fileSize = input.size();
                auto &pool = Streaming::pool(static_cast<int>(fileSize));
                input.read(pool.begin(), fileSize);
                data->valueBlob = pool.commit(fileSize);
            }
        });
        f->fetch(m_basedir, m_currency);
    }
}

void PriceHistoryDataProvider::setAllowLogCompression(bool newAllowLogCompression)
{
    m_allowLogCompression = newAllowLogCompression;
}


// ---------------------------------

InitialHistoryFetcher::InitialHistoryFetcher(QObject *parent)
    : QObject(parent)
{
}

void InitialHistoryFetcher::fetch(const QString &path, const QString &currency)
{
    assert(!path.isEmpty());
    QNetworkRequest req(QUrl("https://flowee.org/products/pay/fiat/" + currency));
    auto app = QCoreApplication::instance();
    QString useragent = QString("%1%2/%3")
            .arg(app->organizationName(),
                 app->applicationName(),
                 app->applicationVersion());
    req.setRawHeader("User-Agent", useragent.toLatin1());
    auto reply = m_network.get(req);
    connect (reply, &QNetworkReply::finished, reply, [=]() {
        // either everything went fine, or the server doesn't have our file.
        // in both cases we will not retry and we create a file.
        if (reply->error() == QNetworkReply::NoError || reply->error() == QNetworkReply::ContentNotFoundError ) {
            QFile out(path + '/' + currency);
            if (out.open(QIODevice::WriteOnly)) {
                out.write(reply->readAll());
            }
            else {
                logWarning() << "Failed to write to fiat file";
            }
        }
        else {
            logWarning() << "Download fiat history failed" << reply->errorString();
        }
        reply->deleteLater();
        emit success(currency);
        this->deleteLater();
    });
}
