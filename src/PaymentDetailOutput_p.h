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
#ifndef PAYMENT_DETAIL_OUTPUT_H
#define PAYMENT_DETAIL_OUTPUT_H

#include "Payment.h"

class PaymentDetailOutput : public PaymentDetail
{
    Q_OBJECT
    /*
     * The user set string which is the address.
     * We verify this string and fill \a formattedTarget only when this is a correct address.
     * Notice that a legacy address will only be seen as correct when the forceLegacyOk is true.
     */
    Q_PROPERTY(QString address READ address WRITE setAddress NOTIFY addressChanged)
    Q_PROPERTY(double paymentAmount READ paymentAmount WRITE setPaymentAmount NOTIFY paymentAmountChanged)
    Q_PROPERTY(qint64 paymentAmountFiat READ paymentAmountFiat WRITE setPaymentAmountFiat NOTIFY paymentAmountFiatChanged)
    // cleaned up and re-formatted, empty if invalid.
    Q_PROPERTY(QString formattedTarget READ formattedTarget NOTIFY correctAddressChanged)
    // same as formatted, but without prefix.
    Q_PROPERTY(QString niceAddress READ niceAddress NOTIFY correctAddressChanged)
    Q_PROPERTY(bool maxAllowed READ maxAllowed WRITE setMaxAllowed NOTIFY maxAllowedChanged)
    Q_PROPERTY(bool fiatFollows READ fiatFollows WRITE setFiatFollows NOTIFY fiatFollowsChanged)
    Q_PROPERTY(bool maxSelected READ maxSelected WRITE setMaxSelected NOTIFY maxSelectedChanged)
    Q_PROPERTY(bool forceLegacyOk READ forceLegacyOk WRITE setForceLegacyOk NOTIFY forceLegacyOkChanged)
    /**
     * An output created by a payment protocol is not editable by users.
     */
    Q_PROPERTY(bool editable READ editable NOTIFY editableChanged)
public:
    explicit PaymentDetailOutput(Payment *parent);

    double paymentAmount() const;
    void setPaymentAmount(double newPaymentAmount);

    /// this method also sets formattedTarget if it's a proper address.
    /// @see FloweePay::identifyString()
    /// @see setOutputScript()
    const QString &address() const;
    void setAddress(const QString &newAddress);
    /// is non-empty if the address() is proper.
    const QString &formattedTarget() const;

    void setOutputScript(const Streaming::ConstBuffer &script);
    Streaming::ConstBuffer outputScript() const;

    /**
     * The nicest to display address version.
     * This will always return (if available) the BCH style (cash-address) address, without
     * the bitcoincash: prefix.
     */
    QString niceAddress() const;

    /**
     * The mayment amount can be set to 'max' which means the wallet-total-value.
     * This returns weather this output can be set to 'max'
     */
    bool maxAllowed() const;
    void setMaxAllowed(bool on);

    qint64 paymentAmountFiat() const;
    void setPaymentAmountFiat(qint64 amount);

    bool fiatFollows() const;
    void setFiatFollows(bool on);

    bool maxSelected() const;
    void setMaxSelected(bool on);

    /// If max is selected, recalc the effective fiat / payment amounts
    void recalcMax();

    bool forceLegacyOk() const;
    void setForceLegacyOk(bool newForceLegacyOk);

    void setWallet(Wallet *wallet) override;

    bool editable() const {
        return m_editable;
    }
    void setEditable(bool on);

signals:
    void paymentAmountChanged();
    void paymentAmountFiatChanged();
    void addressChanged();
    void correctAddressChanged();
    void fiatFollowsChanged();
    void maxSelectedChanged();
    void forceLegacyOkChanged();
    void maxAllowedChanged();
    void editableChanged();

private:
    void checkValid();
    void createFormattedAddress();

    qint64 m_paymentAmount = 0;
    qint64 m_fiatAmount = 0;
    bool m_maxAllowed = true; // only the last in the sequence can have 'max'
    bool m_fiatFollows = false;
    bool m_maxSelected = false;
    bool m_addressOk = false;
    bool m_forceLegacyOk = false;
    bool m_editable = true;
    QString m_address;
    QString m_formattedTarget;
    Streaming::ConstBuffer m_outputScript;
};

inline PaymentDetailOutput* PaymentDetail::toOutput() {
    assert(m_type == Payment::PayToAddress);
    return static_cast<PaymentDetailOutput*>(this);
}

#endif
