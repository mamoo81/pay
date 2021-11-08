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
#include <utils/cashaddr.h>
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
        auto output = wallet->txOutput(ref);
        QCOMPARE(wallet->unlockKey(ref).sigType, Wallet::NotUsedYet);
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

    // since we signed, lets check the signature type
    auto secrets = wallet->walletSecrets();
    for (auto ref : funding.outputs) {
        QCOMPARE(wallet->unlockKey(ref).sigType, Wallet::SignedAsEcdsa);
    }

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
        auto output = wallet->txOutput(ref);
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
    bool found = false;
    for (auto &ref : funding.outputs) {
        // there are 2 outputs, we shall not assume the order of the funding request but just look for the right amount
        auto output = wallet->txOutput(ref);
        if (output.outputValue == 200000) {
            found = true;
            b4.appendInput(wallet->txid(ref), ref.outputIndex());
            b4.appendOutput(output.outputValue - 1673);
            break;
        }
    }
    QVERIFY(found);
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

void TestWallet::testSpam()
{
    auto wallet = createWallet();
    TransactionBuilder b1;
    uint256 prevTxId = uint256S("0x12830924807308721309128309128");
    b1.appendInput(prevTxId, 0);
    b1.appendOutput(1000000);
    b1.pushOutputPay2Address(wallet->nextUnusedAddress());
    Streaming::BufferPool pool;
    Tx t1 = b1.createTransaction(&pool);
    wallet->newTransactions(MockBlockHeader(), 1, { t1 });
    QCOMPARE(wallet->balanceConfirmed(), 1000000);


    // create a new transaction spending the above output and creating at least one 'spam' output.
    TransactionBuilder b2;
    b2.appendOutput(200000);
    b2.pushOutputPay2Address(wallet->nextUnusedAddress());
    b2.appendOutput(547); // Da SPAM.
    b2.pushOutputPay2Address(wallet->nextUnusedAddress());
    b2.appendOutput(290000);
    CKeyID id("99999999999999999999");
    b2.pushOutputPay2Address(id);

    int64_t change = -1;
    auto funding = wallet->findInputsFor(990000, 1, b2.createTransaction(&pool).size(), change);
    for (auto ref : funding.outputs) {
        b2.appendInput(wallet->txid(ref), ref.outputIndex());
        auto output = wallet->txOutput(ref);
        b2.pushInputSignature(wallet->unlockKey(ref).key, output.outputScript, output.outputValue, TransactionBuilder::ECDSA);
    }
    Tx t2 = b2.createTransaction(&pool);
    wallet->newTransactions(MockBlockHeader(), 2, { t2 } );
    QCOMPARE(wallet->balanceConfirmed(), 200000); // does NOT include the 547
    QCOMPARE(wallet->balanceUnconfirmed(), 0);
    QCOMPARE(wallet->balanceImmature(), 0);
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
        auto output = wallet->txOutput(ref);
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
        CKeyID id("66666666666666666666");
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
    QCOMPARE(walletSet.totalSats, 6000000);
    QCOMPARE(walletSet.fee, 150);
    QCOMPARE(change, 2099701);

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
        auto output = wallet->txOutput(ref);
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

void TestWallet::hierarchicallyDeterministic()
{
    const QString southMonkey("south monkey fire corn link estate burger lucky bronze pet chapter lamp");
    {
        auto wallet = createWallet();
        QVERIFY(!wallet->isHDWallet());
        QCOMPARE(wallet->hdWalletMnemonic(), QString());
        QCOMPARE(wallet->hdWalletMnemonicPwd(), QString());
        QCOMPARE(wallet->derivationPath(), QString());

        std::vector<uint32_t> derivationPath = { HDMasterKey::Hardened + 44,  HDMasterKey::Hardened + 145, HDMasterKey::Hardened };
        wallet->createHDMasterKey(southMonkey, QString(), derivationPath);

        QVERIFY(wallet->isHDWallet());
        QCOMPARE(wallet->hdWalletMnemonic(), southMonkey);
        QCOMPARE(wallet->hdWalletMnemonicPwd(), QString());
        QCOMPARE(wallet->derivationPath(), QString("m/44'/145'/0'"));
    }
    {
        auto wallet = openWallet();
        QVERIFY(wallet->isHDWallet());
        QCOMPARE(wallet->hdWalletMnemonic(), southMonkey);
        QCOMPARE(wallet->hdWalletMnemonicPwd(), QString());
        QCOMPARE(wallet->derivationPath(), QString("m/44'/145'/0'"));

        CKeyID id;
        int num = wallet->reserveUnusedAddress(id);
        QCOMPARE(num, 1); // the first maps to the non-HD one created in our unit-test below (in createWallet())

        // Create a new HD wallet, which should use base derivation plus "/0/0" appended.
        num = wallet->reserveUnusedAddress(id);
        QCOMPARE(num, 2);

        QVERIFY(!id.IsNull());
        auto address = CashAddress::encodeCashAddr("bitcoincash",
                { CashAddress::PUBKEY_TYPE, std::vector<uint8_t>(id.begin(), id.end())});
        QCOMPARE(QString::fromStdString(address), "bitcoincash:qrg0jddykyfeal70xduvyeathd3ulhm7hv22m3t7na");
        QCOMPARE(wallet->derivationPath(), QString("m/44'/145'/0'"));
    }
    {
        auto wallet = openWallet();
        QVERIFY(wallet->isHDWallet());

        auto secrets = wallet->walletSecrets();
        bool plain = false; // found the plain one.
        QSet<int> change;
        QSet<int> main;
        for (const auto &i : secrets) {
            const auto &secret = i.second;
            if (secret.fromHdWallet) {
                if (secret.fromChangeChain) {
                    QVERIFY(!change.contains(secret.hdDerivationIndex));
                    change.insert(secret.hdDerivationIndex);
                }
                else {
                    QVERIFY(!main.contains(secret.hdDerivationIndex));
                    main.insert(secret.hdDerivationIndex);
                }
            }
            else {
                QCOMPARE(plain, false);
                plain = true;
            }
        }
        // we found (after loading) at least these many pre-calculated wallet-secrets.
        QVERIFY(change.size() >= 20);
        QVERIFY(main.size() >= 20);
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

void TestWallet::testRef()
{
    Wallet::OutputRef a(10, 100);
    Wallet::OutputRef b(a.encoded());
    QCOMPARE(a.outputIndex(), 100);
    QCOMPARE(b.outputIndex(), 100);
    QCOMPARE(a.txIndex(), 10);
    QCOMPARE(b.txIndex(), 10);

    a.setTxIndex(12);
    Wallet::OutputRef c(a.encoded());
    QCOMPARE(a.outputIndex(), 100);
    QCOMPARE(c.outputIndex(), 100);
    QCOMPARE(a.txIndex(), 12);
    QCOMPARE(c.txIndex(), 12);
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
