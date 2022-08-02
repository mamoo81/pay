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

#include <QTimer>

PriceHistoryDataProvider::PriceHistoryDataProvider(const boost::filesystem::path &basedir, const QString &currency, QObject *parent)
    : QObject{parent},
      m_currency(currency),
      m_basedir(basedir / "fiat")
{
}

void PriceHistoryDataProvider::addPrice(const QString &currency, uint32_t timestamp, int price)
{
    // we add the price always to the LOG file (both the in-memory as well as the disk one)
    Currency *data = nullptr;
    for (auto i = m_currencies.begin(); i != m_currencies.end(); ++i) {
        if (i->id == currency) {
            data = &*i;
            break;
        }
    }
    if (!data) {
        boost::filesystem::create_directories(m_basedir);
        QString logPath("%1/%2.log");
        logPath = logPath.arg(m_basedir.string().c_str());
        logPath = logPath.arg(currency);
        Currency c;
        c.id = currency;
        c.log = new QFile(logPath, this);
        m_currencies.push_back(c);
        data = &m_currencies.back();
        if (!data->log->open(QIODevice::WriteOnly))
            throw std::runtime_error("PriceHistory: Failed to open LOG file for write");
    }
    assert(data);
    assert(data->log);
    assert(price >= 0);
    data->logValues.insert(std::make_pair(timestamp, price));
    char buf[8];
    *reinterpret_cast<uint32_t*>(buf) = le32toh(timestamp);
    *reinterpret_cast<uint32_t*>(buf + 4) = le32toh(price);
    data->log->write(buf, 8);
    data->log->flush();

    if (data->logValues.size() > 50)
        QTimer::singleShot(400, this, SLOT(processLog()));
}

int PriceHistoryDataProvider::historicalPrice(uint32_t timestamp) const
{
    // TODO
    return 0;
}

QString PriceHistoryDataProvider::currencyName() const
{
    return m_currency;
}

void PriceHistoryDataProvider::setCurrency(const QString &newCurrency)
{
    m_currency = newCurrency;
}

void PriceHistoryDataProvider::processLog()
{
    // TODO
}
