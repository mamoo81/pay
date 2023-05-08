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
#ifndef TEST_WALLET_HISTORY_MODEL_H
#define TEST_WALLET_HISTORY_MODEL_H

#include <QObject>
#include <Wallet.h>

class TestWalletHistoryModel : public QObject
{
    Q_OBJECT
public:
    TestWalletHistoryModel();

private slots:
    void cleanup(); // called after each testcase.
    void basic();
    void basic_data();

private:
    template<class T>
    std::unique_ptr<T> createWallet() {
        std::unique_ptr<T> wallet(reinterpret_cast<T*>(Wallet::createWallet(m_dir.toStdString(), 1111, "test")));
        return wallet;
    }
    QString m_dir;
};

#endif
