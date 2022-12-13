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

protected:
    uint32_t secsSinceEpochFor(int blockHeight) const override {
        assert(blockHeight > 0);
        assert(blockHeight <= 6000);
        // lets make our blocks range from 1000 hours ago (a little over 40 days) to now.
        // we define one block every 600 seconds exect, making the 1000 hours take 6000 blocks.
        static auto now = QDateTime::currentSecsSinceEpoch();
        return static_cast<uint32_t>(now - 1000 * 3600) + blockHeight * 600;
    }
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
    auto wallet = createWallet<MockWallet>();
    std::unique_ptr<MockWalletHistoryModel> model(new MockWalletHistoryModel(wallet.get()));
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
     * As the grouping is done on the current date, I need to also verify relative to the
     * current date.
     * The first transaction will always be in "today", but the amount of transactions after
     * that are in that same group is relative to what time it is...
     *
     * This leads the groups to be numbered like this:
     *
     * 0: prev month            [may be twice]
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

    const auto now = QDateTime::currentDateTime();
    auto today = now.date();
    logFatal() << "dayOfWeek" << today.dayOfWeek();
    if (today.dayOfWeek() > 2) {
        // that means we should have a 'EarlierThisWeek' group.

        groupId = model->data(model->index(24 * 6 * (today.dayOfWeek() - 1), 0), WalletHistoryModel::GroupId);
        QVERIFY(groupId.isValid());
        QCOMPARE(model->groupingPeriod(groupId.toInt()), "Earlier this week");
    }
    if (today.day() > 2) {
        // that means we should have a 'EarlierThisMonth' group.

        groupId = model->data(model->index(24 * 6 * (today.day() - 1), 0), WalletHistoryModel::GroupId);
        QVERIFY(groupId.isValid());
        QCOMPARE(model->groupingPeriod(groupId.toInt()), "Earlier this month");
    }

}



QTEST_MAIN(TestWalletHistoryModel)
