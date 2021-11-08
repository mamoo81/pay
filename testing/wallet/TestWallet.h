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
#ifndef TEST_WALLET_H
#define TEST_WALLET_H

#include <BlockHeader.h>
#include <QObject>

#include <utils/primitives/key.h>

struct ECC_State {
    ECC_State() { ECC_Start(); }
    ~ECC_State() { ECC_Stop(); }
};

struct MockBlockHeader : public BlockHeader
{
    MockBlockHeader();
};

class Wallet;

class TestWallet : public QObject
{
    Q_OBJECT
private slots:
    void cleanup(); // called after each testcase.
    void testRef();
    void transactionOrdering();
    void addingTransactions();
    void lockingOutputs();
    void testSpam();
    void saveTransaction();
    void saveTransaction2();
    void findInputs();
    void unconfirmed();

    void hierarchicallyDeterministic();

private:
    std::unique_ptr<Wallet> createWallet();
    std::unique_ptr<Wallet> openWallet();
    QString m_dir;
    const ECC_State m_state;
};

#endif
