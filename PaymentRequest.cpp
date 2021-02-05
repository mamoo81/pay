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
#include "Wallet.h"

#include <QTimer>
#include <QUrl>
#include <base58.h>
#include <cashaddr.h>

PaymentRequest::PaymentRequest(QObject *parent)
    : m_wallet(nullptr)
{
    // we need this constructor to allow the QML system to see it.
    assert(false);
}

PaymentRequest::PaymentRequest(Wallet *wallet, QObject *parent)
  : QObject(parent),
  m_wallet(wallet)
{
    Q_ASSERT(m_wallet);
    m_privKeyId = m_wallet->reserveUnusedAddress(m_address);
    m_wallet->addPaymentRequest(this);
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
}

// constructor used to 'load' a request, already owned by the wallet.
PaymentRequest::PaymentRequest(Wallet *wallet, int /* type */)
    : QObject(wallet),
      m_wallet(wallet),
    m_saveState(Remembered)
{
    // TODO remember type when we start to support more than just BIP21
    m_unusedRequest = false;
}

void PaymentRequest::setPaymentState(PaymentState newState)
{
    if (newState == m_paymentState)
        return;

    m_paymentState = newState;
    m_dirty = true;
    emit paymentStateChanged();
}

qint64 PaymentRequest::amountSeen() const
{
    return m_amountSeen;
}

PaymentRequest::PaymentState PaymentRequest::paymentState() const
{
    return m_paymentState;
}

void PaymentRequest::addPayment(uint64_t ref, int64_t value, int blockHeight)
{
    assert(value > 0);
    if (m_incomingOutputRefs.contains(ref)) {
        if (m_paymentState >= PaymentSeen && blockHeight != -1)
            setPaymentState(Confirmed);
        return;
    }
    m_incomingOutputRefs.append(ref);
    m_amountSeen += value;
    m_dirty = true;

    if (m_paymentState == Unpaid && m_amountSeen >= m_amountRequested) {
        if (blockHeight == -1)  {
            setPaymentState(PaymentSeen);
            QTimer::singleShot(FloweePay::instance()->dspTimeout(), [=]() {
                if (m_paymentState == PaymentSeen)
                    setPaymentState(PaymentSeenOk);
            });
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
    m_dirty = true;
    if (m_paymentState >= PaymentSeen && m_amountSeen < m_amountRequested)
        setPaymentState(Unpaid);
    emit amountSeenChanged();
}

PaymentRequest::SaveState PaymentRequest::saveState() const
{
    return m_saveState;
}

void PaymentRequest::setSaveState(const SaveState &saveState)
{
    if (m_saveState == saveState)
        return;
    m_saveState = saveState;
    m_dirty = true;
    emit saveStateChanged();
}

PaymentRequest::~PaymentRequest()
{
    // free address again, if the ownership hasn't moved to be the wallet itself.
    if (m_unusedRequest) {
        m_wallet->removePaymentRequest(this);
        m_wallet->unreserveAddress(m_privKeyId);
    }
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
    m_dirty = true;
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

double PaymentRequest::amountSeenFP() const
{
    return m_amountSeen;
}

qint64 PaymentRequest::amount() const
{
    return m_amountRequested;
}

QString PaymentRequest::qrCodeString() const
{
    QString rc;
    // add address
    if (m_useLegacyAddressFormat) {
        CBase58Data legacy;
        legacy.setData(m_address, CBase58Data::PubkeyType,
                       FloweePay::instance()->chain() == P2PNet::MainChain
                       ? CBase58Data::Mainnet : CBase58Data::Testnet);
        rc += QString::fromStdString(legacy.ToString());
    }
    else {
        CashAddress::Content c;
        c.hash = std::vector<uint8_t>(m_address.begin(), m_address.end());
        c.type = CashAddress::PUBKEY_TYPE;
        rc += QString::fromStdString(CashAddress::encodeCashAddr(FloweePay::instance()->chainPrefix(), c));
    }

    bool separatorInserted = false; // the questionmark.
    if (m_amountRequested > 0) {
        // Amount is in whole BCHs
        QString price = FloweePay::priceToString(m_amountRequested, FloweePay::BCH);
        int length = price.size() - 1;
        // we strip trailing zero's
        while (length >= 0) {
            if (price.at(length) != '0' && price.at(length) != '.')
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
        rc += QString("message=%1").arg(m_message);
        QUrl url(rc);
        rc = QString::fromLatin1(url.toEncoded());
    }
    return rc;
}

bool PaymentRequest::useLegacyAddress()
{
    return m_useLegacyAddressFormat;
}

void PaymentRequest::setUseLegacyAddress(bool on)
{
    if (on == m_useLegacyAddressFormat)
        return;
    m_useLegacyAddressFormat = on;
    emit legacyChanged();
    emit qrCodeStringChanged();
}

void PaymentRequest::rememberPaymentRequest()
{
    if (!m_unusedRequest)
        return;
    m_unusedRequest = false;
    setParent(m_wallet);
    setSaveState(Remembered);
}

void PaymentRequest::forgetPaymentRequest()
{
    if (m_unusedRequest)
        return;
    m_unusedRequest = true;
    m_wallet->removePaymentRequest(this);
    deleteLater();
}
