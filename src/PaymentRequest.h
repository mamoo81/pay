/*
 * This file is part of the Flowee project
 * Copyright (C) 2020-2023 Tom Zander <tom@flowee.org>
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
#ifndef PAYMENTREQUEST_H
#define PAYMENTREQUEST_H

#include <QObject>

#include <primitives/pubkey.h>

class Wallet;
class AccountInfo;

/**
 * This is a user-created request for payment to a specific wallet/address.
 * The request created by the user is similar to an invoice, but when the payment
 * is fulfilled the data is transferred to the wallet and this request is deleted.
 */
class PaymentRequest : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString address READ address NOTIFY addressChanged)
    Q_PROPERTY(QString message READ message WRITE setMessage NOTIFY messageChanged)
    Q_PROPERTY(QString qr READ qrCodeString NOTIFY qrCodeStringChanged)
    Q_PROPERTY(double amount READ amount WRITE setAmount NOTIFY amountChanged)
    Q_PROPERTY(double amountSeen READ amountSeen NOTIFY amountSeenChanged)
    Q_PROPERTY(PaymentState state READ paymentState NOTIFY paymentStateChanged)
public:
    /// The state of this payment
    enum PaymentState {
        Unpaid,         //< we have not seen any payment yet.
        DoubleSpentSeen,//< We have seen a double-spend-proof (DSP). This is bad.
        PaymentSeen,    //< A payment has been seen, there is still risk.
        PaymentSeenOk,  //< A payment has been seen, we waited and no DSP came.
        Confirmed       //< We got paid.
    };
    Q_ENUM(PaymentState)

    PaymentRequest(QObject *parent = nullptr);

    QString message() const;
    void setMessage(const QString &message);

    /// Set the amount requested (as floating point) in sats.
    void setAmount(double amount);
    /// return the amount requested (in sats)
    double amount() const;
    /// return the amount received (in sats)
    double amountSeen() const;

    QString qrCodeString() const;

    PaymentState paymentState() const;

    /**
     * Add a payment made towards fulfilling the request.
     * @param ref the Wallet reference to the payment.
     * @param value the amount paid, in satoshis.
     * @param blockHeight or -1 when not mined yet
     */
    void addPayment(uint64_t ref, int64_t value, int blockHeight = -1);
    /**
     * Mark a payment as rejected (typically double-spent).
     * @param ref the Wallet reference to the payment.
     * @param value the amount paid, in satoshis.
     */
    void paymentRejected(uint64_t ref, int64_t value);

    Q_INVOKABLE void clear();

    void setWallet(Wallet *wallet);

    QString address() const;
    void setAddress(const QString &newAddress);

signals:
    void messageChanged();
    void qrCodeStringChanged();
    void amountChanged();
    void amountSeenChanged();
    void paymentStateChanged();
    void walletChanged();
    void addressChanged();

private slots:
    void walletEncryptionChanged();

private:
    void setPaymentState(PaymentState newState);

    Wallet *m_wallet;
    QString m_message;
    KeyId m_address;
    int m_privKeyId = -1; // refers to the Wallets list of private keys
    qint64 m_amountRequested = 0;
    qint64 m_amountSeen = 0;
    PaymentState m_paymentState = Unpaid;

    QList<uint64_t> m_incomingOutputRefs; // see Wallet::OutputRef
};

#endif
