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
#ifndef PAYMENTREQUEST_H
#define PAYMENTREQUEST_H

#include <QObject>

#include <primitives/pubkey.h>

class Wallet;

/**
 * This is a user-created request for payment to a specific wallet/address.
 * The request created by the user is similar to an invoice, but when the payment
 * is fulfilled the data is transferred to the wallet and this request is deleted.
 */
class PaymentRequest : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString message READ message WRITE setMessage NOTIFY messageChanged)
    Q_PROPERTY(QString qr READ qrCodeString NOTIFY qrCodeStringChanged)
    Q_PROPERTY(double amount READ amountFP WRITE setAmountFP NOTIFY amountChanged)
    Q_PROPERTY(bool legacy READ useLegacyAddress WRITE setUseLegacyAddress NOTIFY legacyChanged)
public:
    enum State {
        Unpaid,
        PaymentSeen,
        DoubleSpentSeen,
        Confirmed
    };
    explicit PaymentRequest(Wallet *wallet, QObject *parent = nullptr);
    ~PaymentRequest();

    QString message() const;
    void setMessage(const QString &message);
    /*
     * I need a getter for a paymentId, which is the bip21 url, or the bip70 url etc.
     * A call to copy it to the clipboard and something to build a QR image based on it
     * are needed to (sounds like I want to add those as utility methods elsewhere, though).
     *
     * Make State be set by the wallet, as such make Wallet have a list of requests
     * that it pushes incoming transactions to.
     * Ditto for double-spend proofs.
     *
     * Have both an amount and a paid-amount field.
     *
     * Remember creation date and sort the requests.
     * This means that the wallet needs to save these items in its wallet.dat file.
     */

    void setAmountFP(double amount);
    double amountFP() const;
    qint64 amount() const;

    State state() const;
    void setState(const State &state);

    QString qrCodeString() const;

    bool useLegacyAddress();
    void setUseLegacyAddress(bool on);

    Q_INVOKABLE void rememberPaymentRequest();

signals:
    void messageChanged();
    void qrCodeStringChanged();
    void amountChanged();
    void legacyChanged();

protected:
    friend class Wallet;
    explicit PaymentRequest(Wallet *wallet, int paymentType);

private:
    Wallet * const m_wallet;
    QString m_message;
    CKeyID m_address;
    int m_privKeyId = -1; // refers to the Wallets list of private keys
    bool m_unusedRequest = true; ///< true as long as the user did not decide to save the request
    bool m_useLegacyAddressFormat = false;
    State m_state = Unpaid;
    qint64 m_amountRequested = 0;
};

#endif
