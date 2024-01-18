/*
 * This file is part of the Flowee project
 * Copyright (C) 2020-2024 Tom Zander <tom@flowee.org>
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
#ifndef TXINFOOBJECT_H
#define TXINFOOBJECT_H

#include <QObject>
#include <QMutex>
#include <QList>
#include <NetworkEndPoint.h>
#include <p2p/BroadcastTxData.h>


class Wallet;
class Payment;


/**
 * This is used to broadcast transactions.
 *
 * The transaction is given to the p2p library where it will be send to all peers
 * associated with the wallet. It will be a 2 stage effort, first an INV then as the
 * peer asks the actual tx-data.
 *
 * The actual creator of the transaction data should have a shared pointer to this
 * instance and deleting it will stop the broadcast and us learning about reject
 * messages.
 *
 * The Payment object creates a transaction and creates an instance to let the
 * rest of the system know.
 * The wallet will go through a similar process whenever a new block is found and
 * a transaction it holds is not mined (nor made invalid), broadcasting the unconfirmed
 * transaction to all peers.
 * The p2p layer will also send it to new peers the moment they connect (and get
 * associated with this wallet).
 *
 * @see ConnectionManager::broadcastTransaction()
 */
class TxInfoObject : public QObject, public BroadcastTxData
{
    Q_OBJECT
public:
    TxInfoObject(Payment *payment, const Tx &tx);
    TxInfoObject(Wallet *parent, int txIndex, const Tx &tx);

    void sentVia(const std::shared_ptr<Peer> &peer) override;
    void txRejected(int connectionId, RejectReason reason, const std::string &message) override;
    uint16_t privSegment() const override;

    int txIndex() const;

    struct Status {
        int sent = 0; // number of peers we sent this transaction to.
        int inProgress = 0; // number of peers in progress.
        int failed = 0; // number of peers reported failure.
    };
    Status calcStatus(int waitingTimeoutMS = 3000) const;

private slots:
    // called some seconds after the request from a peer for data
    void checkState();

signals:
    void finished(int txIndex, bool success);
    void sentOne();
    void rejectionSeen();

private:
    mutable QMutex m_lock;
    const Wallet *m_wallet;
    int m_txIndex;

    struct Broadcast {
        int connectionId = -1;
        EndPoint ep;
        bool failed = false;
        RejectReason rejected = NoReason;
        std::string rejectMsg;
        uint64_t sentTime = 0; // ms since epoch.
    };
    QList<Broadcast> m_broadcasts;
};


#endif
