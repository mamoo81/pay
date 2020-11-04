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
#ifndef WALLET_P_H
#define WALLET_P_H

#include <BroadcastTxData.h>

#include <QObject>
#include <QSet>
#include <deque>
#include <vector>

class Wallet;

namespace WalletPriv
{
    // we may have transactions that spend outputs created within the same block.
    // this method will make sure that we sort them so those that spend others
    // are sorted after the ones they spent.
    std::deque<Tx> sortTransactions(const std::deque<Tx> &in);
}

// this is used to broadcast transactions
// see ConnectionManager::broadcastTransaction()
class WalletInfoObject : public QObject, public BroadcastTxData
{
    Q_OBJECT
public:
    WalletInfoObject(Wallet *parent, int txIndex, const Tx &tx);
    void txRejected(RejectReason reason, const std::string &message);
    void sentOne();
    uint16_t privSegment() const;

    int txIndex() const;

private slots:
    // called 5 seonds after every request from a peer for data
    void checkState();

signals:
    void finished(int txIndex, bool success);

private:
    const Wallet *m_wallet;
    const int m_txIndex;
    short m_sentPeerCount = 0;
    short m_rejectedPeerCount = 0;
};

#endif
