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
#include "PaymentRequest.h"
#include "FloweePay.h"

PaymentRequest::PaymentRequest(Wallet *wallet, QObject *parent)
  : QObject(parent),
  m_wallet(wallet)
{
    // reserve address here.
}

PaymentRequest::~PaymentRequest()
{
     // free address again, if the ownership hasn't moved to be the wallet itself.
}

QString PaymentRequest::message() const
{
    return m_message;
}

void PaymentRequest::setMessage(const QString &message)
{
    if (m_message == message)
        return;
    m_message = message;
    emit messageChanged();
    emit qrCodeStringChanged();
}

void PaymentRequest::setAmountFP(double amount)
{
    qint64 newAmount = amount;
    if (newAmount == m_amountRequested)
        return;
    m_amountRequested = newAmount;
    emit amountChanged();
    emit qrCodeStringChanged();
}

double PaymentRequest::amountFP() const
{
    return m_amountRequested;
}

qint64 PaymentRequest::amount() const
{
    return m_amountRequested;
}

PaymentRequest::State PaymentRequest::state() const
{
    return m_state;
}

void PaymentRequest::setState(const State &state)
{
    m_state = state;
}

QString PaymentRequest::qrCodeString() const
{
    // TODO use QURL
    QString rc = QString::fromStdString(FloweePay::instance()->chainPrefix());
    rc += ":";
    // TODO add address
    if (!m_message.isEmpty()) {
        rc += QString("&message=%1").arg(m_message);
    }
    if (m_amountRequested > 0) {
        rc += QString("&amount=%1").arg(m_amountRequested);
    }
    return rc;
}
