/*
 * This file is part of the Flowee project
 * Copyright (C) 2020-2021 Tom Zander <tom@flowee.org>
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
#include <Wallet.h>
#include <Wallet_p.h>
#include <FloweePay.h>

#include <QtTest/QtTest>
#include <QStandardPaths>
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

void TestWallet::addingTransactions()
{
    auto wallet = createWallet();
    TransactionBuilder b1;
    uint256 prevTxId = uint256S("0x12830924807308721309128309128");
    b1.appendInput(prevTxId, 0);
    b1.appendOutput(1000000);
    b1.pushOutputPay2Address(wallet->nextUnusedAddress());
    Streaming::BufferPool pool;
    Tx t1 = b1.createTransaction(&pool);
    QCOMPARE(wallet->balanceConfirmed(), 0);
    QCOMPARE(wallet->balanceUnconfirmed(), 0);
    QCOMPARE(wallet->balanceImmature(), 0);
    wallet->newTransaction(t1);
    QCOMPARE(wallet->balanceConfirmed(), 0);
    QCOMPARE(wallet->balanceUnconfirmed(), 1000000);
    QCOMPARE(wallet->balanceImmature(), 0);

    // adding again is ignored.
    wallet->newTransaction(t1);
    QCOMPARE(wallet->balanceConfirmed(), 0);
    QCOMPARE(wallet->balanceUnconfirmed(), 1000000);
    QCOMPARE(wallet->balanceImmature(), 0);

    // now confirm it.
    std::deque<Tx> list;
    list.push_back(t1);
    wallet->newTransactions(MockBlockHeader(), 1, list);
    QCOMPARE(wallet->balanceConfirmed(), 1000000);
    QCOMPARE(wallet->balanceUnconfirmed(), 0);
    QCOMPARE(wallet->balanceImmature(), 0);


    // create a new transaction spending the above output and creating 3 outputs,
    // 2 for me one to a random address.
    //   1000000 -> 200000, 500000 [and 290000 we send elsewhere]
    TransactionBuilder b2;
    b2.appendOutput(200000);
    b2.pushOutputPay2Address(wallet->nextUnusedAddress());
    b2.appendOutput(500000);
    b2.pushOutputPay2Address(wallet->nextUnusedAddress());
    b2.appendOutput(290000);
    CKeyID id("99999999999999999999");
    b2.pushOutputPay2Address(id);

    int64_t change = -1;
    auto funding = wallet->findInputsFor(990000, 1, b2.createTransaction(&pool).size(), change);
    QCOMPARE(funding.outputs.size(), 1L);

    for (auto ref : funding.outputs) {
        b2.appendInput(wallet->txid(ref), ref.outputIndex());
        auto output = wallet->txOutout(ref);
        b2.pushInputSignature(wallet->unlockKey(ref).key, output.outputScript, output.outputValue, TransactionBuilder::ECDSA);
    }
    Tx t2 = b2.createTransaction(&pool);
    QCOMPARE(wallet->balanceConfirmed(), 1000000);
    QCOMPARE(wallet->balanceUnconfirmed(), 0);
    QCOMPARE(wallet->balanceImmature(), 0);
    // add as unconfirmed
    wallet->newTransaction(t2);
    QCOMPARE(wallet->balanceConfirmed(), 0);
    QCOMPARE(wallet->balanceUnconfirmed(), 200000 + 500000);
    QCOMPARE(wallet->balanceImmature(), 0);

    // try to spend it again, if its properly locked, it will not get funded.
    funding = wallet->findInputsFor(990000, 1, t2.size(), change);
    QVERIFY(funding.outputs.empty());

    // create a new transaction extending the unspent chain and spent both
    // UTXOs in the wallet.
    TransactionBuilder b3;
    b3.appendOutput(698700);
    b3.pushOutputPay2Address(wallet->nextUnusedAddress());
    funding = wallet->findInputsFor(698700, 1, b3.createTransaction(&pool).size(), change);
    QCOMPARE(funding.outputs.size(), 2L);
    for (auto ref : funding.outputs) {
        b3.appendInput(wallet->txid(ref), ref.outputIndex());
        auto output = wallet->txOutout(ref);
        b3.pushInputSignature(wallet->unlockKey(ref).key, output.outputScript, output.outputValue, TransactionBuilder::ECDSA);
    }
    Tx t3 = b3.createTransaction(&pool);

    QCOMPARE(wallet->balanceConfirmed(), 0);
    QCOMPARE(wallet->balanceUnconfirmed(), 200000 + 500000);
    QCOMPARE(wallet->balanceImmature(), 0);
    // add as unconfirmed
    wallet->newTransaction(t3);
    QCOMPARE(wallet->balanceConfirmed(), 0);
    QCOMPARE(wallet->balanceUnconfirmed(), 698700);
    QCOMPARE(wallet->balanceImmature(), 0);

    // Create a double spending transaction and spend one of the two UTXOs only
    TransactionBuilder b4;
    // reuse the output we funded t3 with.
    auto ref = funding.outputs.front();
    b4.appendInput(wallet->txid(ref), ref.outputIndex());
    auto output = wallet->txOutout(ref);
    QCOMPARE(output.outputValue, 200000);
    b4.appendOutput(output.outputValue - 1673);
    b4.pushOutputPay2Address(wallet->nextUnusedAddress());
    Tx t4 = b4.createTransaction(&pool);

    // then add the double spend as confirmed.
    //   This should replace the previous one.
    QCOMPARE(wallet->balanceConfirmed(), 0);
    QCOMPARE(wallet->balanceUnconfirmed(), 698700);
    QCOMPARE(wallet->balanceImmature(), 0);
    list.clear();
    list.push_back(t4);
    wallet->newTransactions(MockBlockHeader(), 2, list);
    QCOMPARE(wallet->balanceConfirmed(), 200000 - 1673); // output from b4 is confirmed
    QCOMPARE(wallet->balanceImmature(), 0);
    QCOMPARE(wallet->balanceUnconfirmed(), 500000);
}

void TestWallet::saveTransaction()
{
    Streaming::BufferPool pool;
    // add a simple transaction and see if it saves.
    {
        auto wallet = createWallet();
        TransactionBuilder b1;
        uint256 prevTxId = uint256S("0x12830924807308721309128309128");
        b1.appendInput(prevTxId, 0);
        b1.appendOutput(1000000);
        b1.pushOutputPay2Address(wallet->nextUnusedAddress());
        Tx t1 = b1.createTransaction(&pool);
        QCOMPARE(wallet->balanceConfirmed(), 0);
        QCOMPARE(wallet->balanceUnconfirmed(), 0);
        QCOMPARE(wallet->balanceImmature(), 0);
        wallet->newTransaction(t1);
        QCOMPARE(wallet->balanceConfirmed(), 0);
        QCOMPARE(wallet->balanceUnconfirmed(), 1000000);
        QCOMPARE(wallet->balanceImmature(), 0);
    }

    // check it saved (we have balance) and then
    // add an unconfirmed transaction spending our single UTXO
    {
        auto wallet = openWallet();
        QCOMPARE(wallet->balanceConfirmed(), 0);
        QCOMPARE(wallet->balanceUnconfirmed(), 1000000);
        QCOMPARE(wallet->balanceImmature(), 0);

        TransactionBuilder b2;
        b2.appendOutput(200000);
        b2.pushOutputPay2Address(wallet->nextUnusedAddress());
        b2.appendOutput(790000);
        CKeyID id("99999999999999999999");
        b2.pushOutputPay2Address(id);

        int64_t change = -1;
        auto funding = wallet->findInputsFor(990000, 1, b2.createTransaction(&pool).size(), change);
        QCOMPARE(funding.outputs.size(), 1L);
        auto ref = funding.outputs.front();
        b2.appendInput(wallet->txid(ref), ref.outputIndex());
        auto output = wallet->txOutout(ref);
        b2.pushInputSignature(wallet->unlockKey(ref).key, output.outputScript, output.outputValue, TransactionBuilder::ECDSA);
        Tx t2 = b2.createTransaction(&pool);

        QCOMPARE(wallet->balanceConfirmed(), 0);
        QCOMPARE(wallet->balanceUnconfirmed(), 1000000);
        QCOMPARE(wallet->balanceImmature(), 0);
        // add as unconfirmed
        wallet->newTransaction(t2);
        QCOMPARE(wallet->balanceConfirmed(), 0);
        QCOMPARE(wallet->balanceUnconfirmed(), 200000);
        QCOMPARE(wallet->balanceImmature(), 0);
    }

    // we likely have both transactions, the question is if the output from the first,
    // that is spent by the second, is locked and thus not counted in balances
    {
        auto wallet = openWallet();
        QCOMPARE(wallet->balanceConfirmed(), 0);
        QCOMPARE(wallet->balanceUnconfirmed(), 200000);
        QCOMPARE(wallet->balanceImmature(), 0);
    }
}

void TestWallet::saveTransaction2()
{
    Streaming::BufferPool pool;
    // add a simple transaction and see if it saves.
    {
        auto wallet = createWallet();
        TransactionBuilder b1;
        b1.appendInput(uint256(), 0); // coinbase
        b1.appendOutput(50);
        b1.pushOutputPay2Address(wallet->nextUnusedAddress());
        Tx t1 = b1.createTransaction(&pool);
        std::deque<Tx> list;
        list.push_back(t1);
        wallet->newTransactions(MockBlockHeader(), 10, list);
        QCOMPARE(wallet->balanceConfirmed(), 0);
        QCOMPARE(wallet->balanceUnconfirmed(), 0);
        QCOMPARE(wallet->balanceImmature(), 50);

        TransactionBuilder b2;
        b2.appendInput(uint256(), 0); // coinbase
        b2.appendOutput(51);
        b2.pushOutputPay2Address(wallet->nextUnusedAddress());
        Tx t2 = b2.createTransaction(&pool);
        list[0] = t2;
        wallet->newTransactions(MockBlockHeader(), 50, list);
        QCOMPARE(wallet->balanceConfirmed(), 0);
        QCOMPARE(wallet->balanceUnconfirmed(), 0);
        QCOMPARE(wallet->balanceImmature(), 101);

        TransactionBuilder b3;
        b3.appendInput(t1.createHash(), 0);
        b3.appendOutput(40);
        b3.pushOutputPay2Address(wallet->nextUnusedAddress());
        b3.appendOutput(10);
        CKeyID id("666");
        b3.pushOutputPay2Address(id);
        Tx t3 = b3.createTransaction(&pool);
        list[0] = t3;
        wallet->newTransactions(MockBlockHeader(), 140, list);
        QCOMPARE(wallet->balanceConfirmed(), 40);
        QCOMPARE(wallet->balanceUnconfirmed(), 0);
        QCOMPARE(wallet->balanceImmature(), 51);
    }

    {
        auto wallet = openWallet();
        QCOMPARE(wallet->balanceConfirmed(), 40);
        QCOMPARE(wallet->balanceUnconfirmed(), 0);
        QCOMPARE(wallet->balanceImmature(), 51);
    }
}

void TestWallet::findInputs()
{
    auto wallet = createWallet();
    wallet->addTestTransactions();

    int64_t change = 0;
    // Test that we prefer a simple solution over one with multiple aged coins
    auto walletSet =  wallet->findInputsFor(4000000, 1, 1, change);
    QCOMPARE(walletSet.outputs.size(), 1);
    QCOMPARE(walletSet.totalSats, 5000000);
    QCOMPARE(walletSet.fee, 150);
    QCOMPARE(change, 999850);

}

void TestWallet::unconfirmed()
{
    /*
     * Create a wallet with a coin.
     * Then spend one with a transaction that places new coins in the wallet.
     * Make sure that after reload the balance is proper.
     */
    Streaming::BufferPool pool;
    {
        auto wallet = createWallet();
        TransactionBuilder b1;
        uint256 prevTxId = uint256S("0x12830924807308721309128309128");
        b1.appendInput(prevTxId, 0);
        b1.appendOutput(1000000);
        b1.pushOutputPay2Address(wallet->nextUnusedAddress());
        Tx t1 = b1.createTransaction(&pool);
        QCOMPARE(wallet->balanceConfirmed(), 0);
        QCOMPARE(wallet->balanceUnconfirmed(), 0);
        QCOMPARE(wallet->balanceImmature(), 0);
        wallet->newTransactions(MockBlockHeader(), 10, std::deque<Tx>() = { t1 });
        QCOMPARE(wallet->balanceConfirmed(), 1000000);
        QCOMPARE(wallet->balanceUnconfirmed(), 0);
        QCOMPARE(wallet->balanceImmature(), 0);
    }

    // check it saved (we have balance) and then
    // add an unconfirmed transaction spending our single UTXO, we additionally make sure
    // that this tx creates a new UTXO for our wallet.
    {
        auto wallet = openWallet();
        QCOMPARE(wallet->balanceConfirmed(), 1000000);
        QCOMPARE(wallet->balanceUnconfirmed(), 0);
        QCOMPARE(wallet->balanceImmature(), 0);

        TransactionBuilder b2;
        b2.appendOutput(900000);
        b2.pushOutputPay2Address(wallet->nextUnusedAddress());

        int64_t change = -1;
        auto funding = wallet->findInputsFor(900000, 1, b2.createTransaction(&pool).size(), change);
        QCOMPARE(funding.outputs.size(), 1L);
        auto ref = funding.outputs.front();
        b2.appendInput(wallet->txid(ref), ref.outputIndex());
        auto output = wallet->txOutout(ref);
        b2.pushInputSignature(wallet->unlockKey(ref).key, output.outputScript, output.outputValue, TransactionBuilder::ECDSA);
        Tx t2 = b2.createTransaction(&pool);

        QCOMPARE(wallet->balanceConfirmed(), 1000000);
        QCOMPARE(wallet->balanceUnconfirmed(), 0);
        QCOMPARE(wallet->balanceImmature(), 0);
        // add as unconfirmed
        wallet->newTransaction(t2);
        QCOMPARE(wallet->balanceConfirmed(), 0);
        QCOMPARE(wallet->balanceUnconfirmed(), 900000);
        QCOMPARE(wallet->balanceImmature(), 0);
    }

    // we likely have both transactions, the question is if the output from the first,
    // that is spent by the second, is locked and thus not counted in balances
    {
        auto wallet = openWallet();
        QCOMPARE(wallet->balanceConfirmed(), 0);
        QCOMPARE(wallet->balanceUnconfirmed(), 900000);
        QCOMPARE(wallet->balanceImmature(), 0);
    }

}

std::unique_ptr<Wallet> TestWallet::createWallet()
{
    if (m_dir.isEmpty()) {
        QString basedir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        m_dir = basedir + QString("/floweepay-%1/").arg(QCoreApplication::instance()->applicationPid());
    }

    std::unique_ptr<Wallet> wallet(Wallet::createWallet(m_dir.toStdString(), 1111, "test"));
    wallet->createNewPrivateKey(0);

    return wallet;
}

std::unique_ptr<Wallet> TestWallet::openWallet()
{
    Q_ASSERT(!m_dir.isEmpty());
    std::unique_ptr<Wallet> wallet(new Wallet(m_dir.toStdString(), 1111));
    return wallet;
}

void TestWallet::cleanup()
{
    if (m_dir.isEmpty())
        return;
    QDir dir(m_dir);
    dir.removeRecursively();
    m_dir.clear();
}



MockBlockHeader::MockBlockHeader()
{
    // not entirely illogical values
    nVersion = 2;
    hashPrevBlock = uint256S("0120120120120");
    hashMerkleRoot = uint256S("12b90980918230a");
    nTime = 1616233780;
    nBits = 1;
    nNonce = 6;
}

QTEST_MAIN(TestWallet)
