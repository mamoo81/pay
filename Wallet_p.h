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
#ifndef FLOWEE_WALLET_P_H
#define FLOWEE_WALLET_P_H

#include <primitives/FastTransaction.h>

#include <QSet>
#include <deque>
#include <vector>


namespace WalletPriv
{
    // we may have transactions that spend outputs created within the same block.
    // this method will make sure that we sort them so those that spend others
    // are sorted after the ones they spent.
    std::deque<Tx> sortTransactions(const std::deque<Tx> &in);
};

#endif
