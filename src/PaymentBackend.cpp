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
#include "PaymentBackend.h"
#include "FloweePay.h"
#include "PriceDataProvider.h"

PaymentBackend::PaymentBackend(QObject *parent)
    : QObject{parent}
{

}

double PaymentBackend::paymentAmount() const
{
    return m_paymentAmount;
}

void PaymentBackend::setPaymentAmount(double amount)
{
    if (qFuzzyCompare(m_paymentAmount, amount))
        return;
    if (amount < 0)
        return;
    m_paymentAmount = amount;
    auto priceInFiat = FloweePay::instance()->prices()->price() * amount / 1E8;
    m_paymentAmountFiat = std::round(priceInFiat);
    emit amountChanged();
}

int PaymentBackend::paymentAmountFiat() const
{
    return m_paymentAmountFiat;
}

void PaymentBackend::setPaymentAmountFiat(int amount)
{
    if (m_paymentAmountFiat == amount)
        return;
    if (amount < 0)
        return;
    m_paymentAmountFiat = amount;
    m_paymentAmount = amount * 1E8 / FloweePay::instance()->prices()->price();
    emit amountChanged();
}
