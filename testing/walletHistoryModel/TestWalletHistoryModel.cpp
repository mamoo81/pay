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
#include "TestWalletHistoryModel.h"
#include <Wallet.h>
#include <WalletHistoryModel.h>

#include <QtTest/QtTest>

class MockWalletHistoryModel : public WalletHistoryModel
{
public:
    explicit MockWalletHistoryModel(Wallet *wallet)
        : WalletHistoryModel(wallet) {}

    void notifyNewTransactions(int firstNew, int count) {
        appendTransactions(firstNew, count);
    }

    void setMockDate(const QDate &newMockDate) {
        m_mockDate = newMockDate;
        createMap();
    }

protected:
    uint32_t secsSinceEpochFor(int blockHeight) const override {
        assert(blockHeight > 0);
        assert(blockHeight <= 6000);
        // lets make our blocks range from 1000 hours ago (a little over 40 days) to 'mockdate'.
        // we define one block every 600 seconds exect, making the 1000 hours take 6000 blocks.
        assert(m_mockDate.isValid());
        QDateTime d(m_mockDate, QTime(12, 0, 0));
        const auto mockSecsSinceEpoch = d.toSecsSinceEpoch();
        return static_cast<uint32_t>(mockSecsSinceEpoch - 1000 * 3600) + blockHeight * 600;
    }
    QDate today() const override {
        assert(m_mockDate.isValid());
        return m_mockDate;
    }

private:
    QDate m_mockDate;
};

class MockWallet : public Wallet
{
public:
    void createTransactions1() {
        WalletTransaction wtx;
        for (int i = m_nextWalletTransactionId; i <= 6000; ++i) {
            wtx.minedBlockHeight = i;
            m_walletTransactions.insert(std::make_pair(m_nextWalletTransactionId++, wtx));
        }
    }
};

TestWalletHistoryModel::TestWalletHistoryModel()
{
    QString basedir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    m_dir = basedir + QString("/floweepay-%1/").arg(QCoreApplication::instance()->applicationPid());
}

void TestWalletHistoryModel::cleanup()
{
    if (m_dir.isEmpty())
        return;
    QDir dir(m_dir);
    dir.removeRecursively();
    m_dir.clear();
}


void TestWalletHistoryModel::basic()
{
    QFETCH(QDate, date);
    QFETCH(QList<QDate>, thisWeek);
    QFETCH(QList<QDate>, thisMonth);

    assert(date.isValid());
    logCritical() << "Our TODAY:" << date.toString();

    auto wallet = createWallet<MockWallet>();
    std::unique_ptr<MockWalletHistoryModel> model(new MockWalletHistoryModel(wallet.get()));
    model->setMockDate(date);
    QCOMPARE(model->rowCount(), 0);
    wallet->createTransactions1();
    model->notifyNewTransactions(1, 6000); // wallet starts counting at 1
    QCOMPARE(model->rowCount(), 6000);

    auto first = model->data(model->index(0, 0), WalletHistoryModel::MinedHeight);
    QVERIFY(first.isValid());
    QCOMPARE(first.toInt(), 6000);

    first = model->data(model->index(0, 0), WalletHistoryModel::GroupId);
    QVERIFY(first.isValid());
    /*
     * The first transaction will always be in "today", since the mock-model
     * hardcodes that we have 12 hours in todays date and thus 12 * 6 = 72 blocks.
     * But the amount of transactions after that are in that same group is relative
     * to the picked date. Which is the reason we test various dates.
     *
     * This leads the groups to be numbered like this:
     *
     * 0: some month            [may be twice]
     * 1: earlier this month    [optional]
     * 2: earlier this week     [optional]
     * 3: yesterday             [optional]
     * 4: today
     *
     * Since we occupy 40 days, the spread may be any combination of those.
     * i.e. 'today' may be just group-id 2 in case today is the first of this month.
     */
    QVERIFY(first.toInt() >= 2);
    QCOMPARE(model->groupingPeriod(first.toInt()), "Today");

    auto groupId = model->data(model->index(24 * 6, 0), WalletHistoryModel::GroupId);
    QVERIFY(groupId.isValid());
    QVERIFY(first.toInt() > groupId.toInt());
    QCOMPARE(model->groupingPeriod(groupId.toInt()), "Yesterday");

#if 0
    groupId = -1;
    for (int i = 0; i < 6000; ++i) {
        int x = model->data(model->index(i, 0), WalletHistoryModel::GroupId).toInt();
        if (x != groupId.toInt()) {
            groupId = x;
            logFatal() << i << model->groupingPeriod(x) << model->data(model->index(i, 0),
                    WalletHistoryModel::MinedDate).toDate().toString();
        }
    }
#endif

    for (const auto &dayInWeek : thisWeek) {
        QVERIFY(dayInWeek.isValid());
        int days = dayInWeek.daysTo(date);
        groupId = model->data(model->index(24 * 6 * days, 0), WalletHistoryModel::GroupId);
        QVERIFY(groupId.isValid());
        QCOMPARE(model->groupingPeriod(groupId.toInt()), "Earlier this week");
    }

    for (const auto &someDay : thisMonth) {
        QVERIFY(someDay.isValid());
        int days = someDay.daysTo(date);
        groupId = model->data(model->index(24 * 6 * days, 0), WalletHistoryModel::GroupId);
        QVERIFY(groupId.isValid());
        QCOMPARE(model->groupingPeriod(groupId.toInt()), "Earlier this month");
    }
}

void TestWalletHistoryModel::basic_data()
{
    QTest::addColumn<QDate>("date");
    QTest::addColumn<QList<QDate> >("thisWeek"); // any dates where we expect 'earlier this week' labels.
    QTest::addColumn<QList<QDate> >("thisMonth"); // any dates where we expect 'earlier this month' labels.

    QTest::newRow("basic") << QDate(2023, 1, 1)
            << QList<QDate>() // no 'earlier this week'
            << QList<QDate>(); // no 'earlier this month'

    QTest::newRow("end") << QDate(2023, 2, 28)
            << QList<QDate>() // no 'earlier this week'
            << QList<QDate> {QDate(2023, 2, 1), QDate(2023, 2, 26)};

    QTest::newRow("edge") << QDate(2023, 3, 1)
            << QList<QDate> { QDate(2023, 2 , 27) }
            << QList<QDate>(); // no 'earlier this month'

    QTest::newRow("edge2") << QDate(2023, 3, 2)
            << QList<QDate> { QDate(2023, 2 , 27),  QDate(2023, 2 , 28) }
            << QList<QDate>(); // no 'earlier this month'

    QTest::newRow("startWeekNotMonth") << QDate(2023, 5, 7)
            << QList<QDate> { QDate(2023, 5 , 1),  QDate(2023, 5 , 2),  QDate(2023, 5 , 3),  QDate(2023, 5 , 4)   ,  QDate(2023, 5 , 5) }
            << QList<QDate>(); // no 'earlier this month'
}

QTEST_MAIN(TestWalletHistoryModel)
