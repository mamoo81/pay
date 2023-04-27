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
#include "PaymentRequest.h"
#include "FloweePay.h"
#include "Wallet.h"
#include "AccountInfo.h"

#include <QTimer>
#include <QUrl>
#include <base58.h>
#include <cashaddr.h>

PaymentRequest::PaymentRequest(QObject *parent)
    : QObject(parent),
    m_wallet(nullptr)
{
#if 0
    // by enabling this you can simulate the payment request being fulfilled
    QTimer::singleShot(5000, [=]() {
        setPaymentState(PaymentSeen);
        QTimer::singleShot(3000, [=]() {
            // setPaymentState(PaymentSeenOk);
            setPaymentState(DoubleSpentSeen);
        });
    });
#endif

    connect (this, &PaymentRequest::paymentStateChanged, this, [=]() {
        if (m_paymentState == PaymentSeen) {
            // unconfirmed fully paid, but lets see in a couple of seconds...
            QTimer::singleShot(FloweePay::instance()->dspTimeout(), this, [=]() {
                // A DSP can change the status, if its still unchanged then we
                // can safely move to the 'Ok' state.
                if (m_paymentState == PaymentSeen)
                    setPaymentState(PaymentSeenOk);
            });
        }
    }, Qt::QueuedConnection);
}

void PaymentRequest::setPaymentState(PaymentState newState)
{
    if (newState == m_paymentState)
        return;

    m_paymentState = newState;
    emit paymentStateChanged();
}

QString PaymentRequest::address() const
{
    if (m_address.IsNull())
        return QString();
    CashAddress::Content c;
    c.hash = std::vector<uint8_t>(m_address.begin(), m_address.end());
    c.type = CashAddress::PUBKEY_TYPE;
    return QString::fromStdString(CashAddress::encodeCashAddr(FloweePay::instance()->chainPrefix(), c));
}

void PaymentRequest::setWallet(Wallet *wallet)
{
    if (m_wallet == wallet)
        return;

    if (!m_incomingOutputRefs.isEmpty())
        throw std::runtime_error("Not allowed to change wallet on partially fulfilled request");

    const bool closedWAllet = wallet->encryption() == Wallet::FullyEncrypted && !wallet->isDecrypted();
    if (closedWAllet)
        throw std::runtime_error("Payment Request can't use an encrypted wallet");

    if (m_wallet) {
        disconnect (m_wallet, SIGNAL(encryptionChanged()), this, SLOT(walletEncryptionChanged()));
        if (m_paymentState == Unpaid)
            m_wallet->unreserveAddress(m_privKeyId);
    }

    m_wallet = wallet;
    if (m_wallet) {
        m_privKeyId = m_wallet->reserveUnusedAddress(m_address);
        connect (m_wallet, SIGNAL(encryptionChanged()), this, SLOT(walletEncryptionChanged()));
    }
    emit walletChanged();
    emit qrCodeStringChanged();
}

void PaymentRequest::walletEncryptionChanged()
{
    assert (m_wallet);
    const bool closedWAllet = m_wallet->encryption() == Wallet::FullyEncrypted && !m_wallet->isDecrypted();
    if (closedWAllet)
        setWallet(nullptr);
    emit qrCodeStringChanged();
}

PaymentRequest::PaymentState PaymentRequest::paymentState() const
{
    return m_paymentState;
}

void PaymentRequest::addPayment(uint64_t ref, int64_t value, int blockHeight)
{
    assert(value > 0);
    if (m_wallet->isSingleAddressWallet()) {
        // This is a completely empty payment-request and since we are
        // connected to a single-address wallet it is common for transactions
        // to arrive that match our 'reserved' address.
        // This means that we have zero indication that incoming transactions
        // are in response to this request.

        // Just return and keep us showing a QR of our only address.
        return;
    }
    if (m_incomingOutputRefs.contains(ref)) {
        if (m_paymentState >= PaymentSeen && blockHeight != -1)
            setPaymentState(Confirmed);
        return;
    }
    m_incomingOutputRefs.append(ref);
    m_amountSeen += value;

    if (m_paymentState == Unpaid && m_amountSeen >= m_amountRequested) {
        if (blockHeight == -1)  {
            setPaymentState(PaymentSeen);
        } else {
            setPaymentState(Confirmed);
        }
    }
    emit amountSeenChanged();
}

void PaymentRequest::paymentRejected(uint64_t ref, int64_t value)
{
    if (!m_incomingOutputRefs.contains(ref))
        return;
    m_amountSeen -= value;
    if (m_paymentState >= PaymentSeen && m_amountSeen < m_amountRequested)
        setPaymentState(Unpaid);
    emit amountSeenChanged();
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

void PaymentRequest::setAmount(double amount)
{
    qint64 newAmount = amount;
    if (newAmount == m_amountRequested)
        return;
    m_amountRequested = newAmount;
    emit amountChanged();
    emit qrCodeStringChanged();
}

double PaymentRequest::amount() const
{
    return m_amountRequested;
}

double PaymentRequest::amountSeen() const
{
    return m_amountSeen;
}

QString PaymentRequest::qrCodeString() const
{
    if (m_address.IsNull())
        return QString();
    QString rc = address();
    bool separatorInserted = false; // the questionmark.
    if (m_amountRequested > 0) {
        // Amount is in whole BCHs
        QString price = FloweePay::amountToString(m_amountRequested, FloweePay::BCH);
        // in case locale states we use commas:
        price.replace(',', '.');
        int length = price.size() - 1;
        // we strip trailing zero's
        while (length >= 0) {
            if (price.at(length) == '.') {
                --length;
                break;
            }
            if (price.at(length) != '0')
                break;
            --length;
        }
        rc += QString("?amount=%1").arg(price.left(length + 1));
        separatorInserted = true;
    }
    if (!m_message.isEmpty()) {
        if (separatorInserted)
            rc += "&";
        else
            rc += "?";
        rc += QString("label=%1").arg(m_message);
        QUrl url(rc);
        rc = QString::fromLatin1(url.toEncoded());
    }
    return rc;
}

void PaymentRequest::clear()
{
    m_wallet = nullptr;
    m_message.clear();
    m_address = KeyId();
    m_privKeyId = -1;
    m_amountRequested = 0;
    m_amountSeen = 0;
    m_paymentState = Unpaid;
    m_incomingOutputRefs.clear();
}
