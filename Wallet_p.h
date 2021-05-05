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

enum PrivateSaveTags {
    Separator = 0,
    Index,
    PrivKey,
    PrivKeyEncrypted,
    PubKeyHash,
    HeightCreated, // for an address / privkey this is used to check which blocks to query

    IsSingleAddressWallet, // bool
    UserOwnedWallet,       // bool (defaults to true)

    SignatureType,       // See the enum Wallet::WalletSecret::SignatureType
};

enum WalletDataSaveTags {
    // Separator = 0
    // Index = 1
    LastSynchedBlock = 2,
    WalletName,

    // per tx-data
    TxId = 10,
    BlockHash,
    BlockHeight,

    InputIndex,
    InputSpendsTx,          // int that refers to index of the WTX it spends
    InputSpendsOutput,      // input that refers to the output in the WTX it spends

    /// Used after InputIndex to mark the lock-state of an input.
    OutputLockState,         // positive number, see OutputLockStateEnum
    OutputLockAutoSpender,   // the tx that locked an output (index in m_walletTransactions

    // per tx metadata
    UserComment,            // string

    OutputIndex = 30,
    OutputValue,            // in Satoshi
    OutputFromCoinbase,     // bool
    KeyStoreIndex,          // int that refers to the index of the privkey for the current tx-output.

    // PaymentRequests
    PaymentRequestType = 40,   // int, BIP21 or other
    PaymentRequestAddress,     // int, the ID of the private-key
    PaymentRequestMessage,     // string
    PaymentRequestAmount,      // long, num satoshis
    PaymentRequestOldAddress,  // bool (default false)

    PaymentRequestPaid,        // long, num satoshis received
    PaymentRequestTxIndex,     // int, index in m_walletTransactions
    PaymentRequestOutputIndex, // int. Together with prev makes an OutputRef

};

enum OutputLockStateEnum {
    Unlocked = 0, // default value when not saved
    AutoLocked,   // Locked due to it being spent. Locked until mined.
    UserLocked    // User locked this output
};

enum SpecialBlockHeight {
    Genesis = 0,
    Unconfirmed = -1,   // a tx marked with this height is waiting to get confirmed
    Rejected = -2       // a tx marked with this height can no longer be mined
};

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
    // scheduled from a non-Qt thread to start a timer.
    void startCheckState();
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
