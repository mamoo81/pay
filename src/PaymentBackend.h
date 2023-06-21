/*
 * This file is part of the Flowee project
 * Copyright (C) 2023 Tom Zander <tom@flowee.org>
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
#ifndef PAYMENTBACKEND_H
#define PAYMENTBACKEND_H

#include <QObject>

class PaymentBackend : public QObject
{
    Q_OBJECT
    /// The payment-wide amount of funds.
    Q_PROPERTY(double paymentAmount READ paymentAmount WRITE setPaymentAmount NOTIFY amountChanged)
    /// The payment-wide amount of funds in globally set fiat.
    Q_PROPERTY(int paymentAmountFiat READ paymentAmountFiat WRITE setPaymentAmountFiat NOTIFY amountChanged)
public:
    explicit PaymentBackend(QObject *parent = nullptr);

    double paymentAmount() const;
    void setPaymentAmount(double amount);

    int paymentAmountFiat() const;
    void setPaymentAmountFiat(int amount);

signals:
    void amountChanged();

private:
    double m_paymentAmount = 0;
    int m_paymentAmountFiat = 0;
};

#endif
