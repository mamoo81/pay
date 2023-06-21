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
    if (amount < 0)
        return;
    auto priceInFiat = std::round(FloweePay::instance()->prices()->price() * amount / 1E8);
    if (qFuzzyCompare(m_paymentAmount, amount) && m_paymentAmountFiat == priceInFiat)
        return;
    m_paymentAmount = amount;
    m_paymentAmountFiat = priceInFiat;
    emit amountChanged();
}

int PaymentBackend::paymentAmountFiat() const
{
    return m_paymentAmountFiat;
}

void PaymentBackend::setPaymentAmountFiat(int amount)
{
    if (amount < 0)
        return;
    auto satsAmount = amount * 1E8 / FloweePay::instance()->prices()->price();
    if (m_paymentAmountFiat == amount && m_paymentAmount == satsAmount)
        return;
    m_paymentAmountFiat = amount;
    m_paymentAmount = satsAmount;
    emit amountChanged();
}
