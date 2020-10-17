/*
 * This file is part of the Flowee project
 * Copyright (C) 2020 Tom Zander <tomz@freedommail.ch>
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
#include "TestWallet.h"
#include <Wallet_p.h>

#include <QtTest/QtTest>
#include <TransactionBuilder.h>

void TestWallet::transactionOrdering()
{
    std::deque<Tx> unsortedList;
    TransactionBuilder b1;
    uint256 prevTxId = uint256S("0x12830924807308721309128309128");
    b1.appendInput(prevTxId, 0);
    b1.appendOutput(10000);
    Streaming::BufferPool pool;
    Tx t1 = b1.createTransaction(&pool);

    TransactionBuilder b2;
    b2.appendInput(t1.createHash(), 0); // t2 spends an output of t1
    b2.appendOutput(10000);
    Tx t2 = b2.createTransaction(&pool);


    {
        std::deque<Tx> transactions;
        transactions.push_back(t1);
        transactions.push_back(t2);

        auto answer = WalletPriv::sortTransactions(transactions);

        QCOMPARE(answer.size(), 2l);
        QCOMPARE(answer.at(0).createHash(), t1.createHash());
        QCOMPARE(answer.at(1).createHash(), t2.createHash());
    }
    {
        std::deque<Tx> transactions;
        transactions.push_back(t2);
        transactions.push_back(t1);

        auto answer = WalletPriv::sortTransactions(transactions);
        QCOMPARE(answer.size(), 2l);
        QCOMPARE(answer.at(0).createHash(), t1.createHash());
        QCOMPARE(answer.at(1).createHash(), t2.createHash());
    }
}

QTEST_MAIN(TestWallet)
