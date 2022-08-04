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
#include "TestPriceHistory.h"
#include <PriceHistoryDataProvider.h>

#include <QStandardPaths>
#include <QtTest/QtTest>


void TestPriceHistory::cleanup()
{
    if (m_dir.isEmpty())
        return;
    QDir dir(m_dir);
    dir.removeRecursively();
}

void TestPriceHistory::testLog()
{
    const uint32_t TimeStamp = 1650000000;
    PriceHistoryDataProvider ph(basedir().toStdString(), "euro");
    QCOMPARE(ph.historicalPrice(TimeStamp), 0);
    ph.addPrice("euro", TimeStamp, 12345);
    QCOMPARE(ph.historicalPrice(TimeStamp), 12345);
    QCOMPARE(ph.historicalPrice(TimeStamp + 10000), 12345);
    QCOMPARE(ph.historicalPrice(TimeStamp - 10000), 12345);

    ph.addPrice("euro", TimeStamp + 10000, 22345);
    QCOMPARE(ph.historicalPrice(TimeStamp), 12345);
    QCOMPARE(ph.historicalPrice(TimeStamp - 10000), 12345);
    QCOMPARE(ph.historicalPrice(TimeStamp + 4000), 12345);
    QCOMPARE(ph.historicalPrice(TimeStamp + 5001), 22345);
    QCOMPARE(ph.historicalPrice(TimeStamp + 99999), 22345);
}

class MockPHDP : public PriceHistoryDataProvider
{
public:
    void compressLog() { processLog(); }
};

void TestPriceHistory::testFromBlob()
{
    const uint32_t TimeStampBase = 1650000000;
    const uint32_t Day = 60 * 60 * 24;
    {
        PriceHistoryDataProvider ph(basedir().toStdString(), "euro");
        for (int i = 0; i < 10; ++i) {
            ph.addPrice("euro", TimeStampBase + Day * i, i + 1);
        }
        static_cast<MockPHDP*>(&ph)->compressLog(); // hack to access protected method

        for (int i = 5; i < 10; ++i) {
            QCOMPARE(ph.historicalPrice(TimeStampBase + Day * i), i + 1);
        }
    }
    {
        PriceHistoryDataProvider ph(basedir().toStdString(), "euro");
        for (int i = 0; i < 10; ++i) {
            QCOMPARE(ph.historicalPrice(TimeStampBase + Day * i), i + 1);
        }
    }
}

QString TestPriceHistory::basedir()
{
    if (m_dir.isEmpty()) {
        QString basedir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        m_dir = basedir + QString("/floweepay-%1/").arg(QCoreApplication::instance()->applicationPid());
    }
    return m_dir;
}

QTEST_MAIN(TestPriceHistory)
