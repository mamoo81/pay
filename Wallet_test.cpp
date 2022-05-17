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
#include "Wallet.h"

/*
 * Warning; this file is only compiled for unit tests.
 * Nothing in this file will be seen by any released applications.
 */

void Wallet::addTestTransactions()
{
    QMutexLocker locker(&m_lock);
    while (m_walletSecrets.size() < 10) {
        createNewPrivateKey(0);
    }

    uint64_t total = 0;
    for (int i = 0; i < 6; ++i) {
        WalletTransaction wtx;
        wtx.minedBlockHeight = 10;
        Output output;
        output.value = 1000000;
        int outputIndex = 3;
        switch (i) {
        case 0:
            output.walletSecretId = 1;
            output.value = 100000;
            wtx.minedBlockHeight = 1;
            wtx.txid = uint256S("7690458423233793949274923986540813794798131233949274923986540813");
            break;
        case 1:
            output.walletSecretId = 2;
            output.value = 5000000;
            wtx.minedBlockHeight = 10;
            wtx.txid = uint256S("7690458423233792103818319038080");
            break;
        case 2:
            output.value = 400000;
            output.walletSecretId = 3;
            wtx.txid = uint256S("0498b98890485092431083023081830913");
            wtx.minedBlockHeight = 20;
            break;
        case 3:
            output.value = 400000;
            output.walletSecretId = 10;
            wtx.txid = uint256S("549843275584902321873108027832");
            wtx.minedBlockHeight = 20;
            break;
        case 4:
            output.walletSecretId = 4;
            output.value = 1000000;
            wtx.minedBlockHeight = 25;
            wtx.txid = uint256S("709582732308923840923821832908");
            break;
        case 5:
            output.walletSecretId = 5;
            output.value = 6000000;
            wtx.minedBlockHeight = 35;
            wtx.txid = uint256S("594239089060435802934890119830");
            break;
        }
        total += output.value;
        wtx.outputs.insert(std::make_pair(outputIndex, output));
        m_walletTransactions.insert(std::make_pair(m_nextWalletTransactionId, wtx));
        m_txidCache.insert(std::make_pair(wtx.txid, m_nextWalletTransactionId));
        m_unspentOutputs.insert(std::make_pair(OutputRef(m_nextWalletTransactionId, outputIndex).encoded(), output.value));
        ++m_nextWalletTransactionId;
    }
    logCritical() << "Total dummy outputs deposited" << total << "sats";
}
