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
#ifndef PAYMENT_H
#define PAYMENT_H

#include <QObject>
#include <BroadcastTxData.h>
#include <primitives/FastTransaction.h>
#include <memory>

class Wallet;

class Payment : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int feePerByte READ feePerByte WRITE setFeePerByte NOTIFY feePerByteChanged)
    Q_PROPERTY(double paymentAmount READ paymentAmount WRITE setPaymentAmount NOTIFY amountChanged)
    Q_PROPERTY(QString targetAddress READ targetAddress WRITE setTargetAddress NOTIFY targetAddressChanged)
    // cleaned up and re-formatted
    Q_PROPERTY(QString formattedTargetAddress READ formattedTargetAddress NOTIFY targetAddressChanged)
    Q_PROPERTY(QString txid READ txid NOTIFY txCreated)
    Q_PROPERTY(int assignedFee READ assignedFee NOTIFY txCreated)
    Q_PROPERTY(int txSize READ txSize NOTIFY txCreated)
public:
    Payment(Wallet *wallet, qint64 amountToPay);

    void setFeePerByte(int sats);
    int feePerByte();

    void setPaymentAmount(double amount);
    double paymentAmount();

    /// this method throws if its not a proper address.
    /// @see FloweePay::identifyString()
    void setTargetAddress(const QString &address);
    QString targetAddress();
    QString formattedTargetAddress();

    Q_INVOKABLE void approveAndSign();
    Q_INVOKABLE void sendTx();

    /// return the txid, should there be a transaction (otherwise empty string)
    QString txid() const;

    int assignedFee() const;
    int txSize() const;

    Wallet *wallet() const;


private slots:
    void sentToPeer();
    void txRejected(short reason, const QString &message);

signals:
    void feePerByteChanged();
    void amountChanged();
    void targetAddressChanged();
    /// notify how many peers we relayed the transaction to.
    void sent(int count);

    void txCreated();

private:
    Wallet *m_wallet;
    Tx m_tx;
    int m_fee = 1; // in sats per byte
    int m_assignedFee = 0;
    qint64 m_paymentAmount;
    QString m_address;
    QString m_formattedTarget;
    std::shared_ptr<BroadcastTxData> m_infoObject;
    short m_sentPeerCount = 0;
    short m_rejectedPeerCount = 0;
};

#endif
