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
#include <primitives/PublicKey.h>

#include "WalletKeyView.h"

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
    Q_PROPERTY(QString addressShort READ addressShort NOTIFY addressChanged)
    Q_PROPERTY(QString message READ message WRITE setMessage NOTIFY messageChanged)
    Q_PROPERTY(QString qr READ qrCodeString NOTIFY qrCodeStringChanged)
    Q_PROPERTY(double amount READ amount WRITE setAmount NOTIFY amountChanged)
    Q_PROPERTY(double amountSeen READ amountSeen NOTIFY amountSeenChanged)
    Q_PROPERTY(PaymentState state READ paymentState NOTIFY paymentStateChanged)
    Q_PROPERTY(FailReason failReason READ failReason NOTIFY failReasonChanged)
    Q_PROPERTY(QObject* account READ account WRITE setAccount NOTIFY accountChanged)

public:
    /// The state of this payment
    enum PaymentState {
        Unpaid,         //< we have not seen any payment yet.
        PartiallyPaid,  //< we have seen some payments, not full amount
        DoubleSpentSeen,//< We have seen a double-spend-proof (DSP). This is bad.
        PaymentSeen,    //< A payment has been seen, there is still risk.
        PaymentSeenOk,  //< A payment has been seen, we waited and no DSP came.
        Confirmed       //< We got paid.
    };
    Q_ENUM(PaymentState)

    enum FailReason {
        NoFailure,
        NoAccountSet,
        AccountEncrypted,
        AccountImporting
    };
    Q_ENUM(FailReason)

    PaymentRequest(QObject *parent = nullptr);
    ~PaymentRequest();

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
     * For this request and the set properties, reserve an address and show a QR
     */
    Q_INVOKABLE void start();
    /**
     * Forget all details, the account and become clean.
     */
    Q_INVOKABLE void clear();

    QObject *account() const;
    void setAccount(QObject *account);

    QString address() const;
    QString addressShort() const;
    void setAddress(const QString &newAddress);

    PaymentRequest::FailReason failReason() const;
    void setFailReason(FailReason reason);

signals:
    void messageChanged();
    void qrCodeStringChanged();
    void amountChanged();
    void amountSeenChanged();
    void paymentStateChanged();
    void accountChanged();
    void addressChanged();
    void failReasonChanged();

private:
    void setPaymentState(PaymentState newState);
    void updateFailReason();

    WalletKeyView *m_view = nullptr;
    AccountInfo *m_account = nullptr;
    QString m_message;
    KeyId m_address;
    int m_privKeyId = -1; // refers to the Wallets list of private keys
    qint64 m_amountRequested = 0;
    qint64 m_amountSeen = 0;
    PaymentState m_paymentState = Unpaid;
    FailReason m_failReason = NoAccountSet;
};

#endif
