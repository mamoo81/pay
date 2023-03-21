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
}

PaymentRequest::PaymentRequest(Wallet *wallet, QObject *parent)
  : QObject(parent),
  m_wallet(nullptr)
{
    assert(wallet);
    setWallet(wallet);
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
    m_saveState(Stored)
{
    // TODO remember type when we start to support more than just BIP21
    m_unusedRequest = false;
}

PaymentRequest::~PaymentRequest()
{
    // free address again, if the ownership hasn't moved to be the wallet itself.
    if (m_unusedRequest)
        setWallet(nullptr);
}


void PaymentRequest::setPaymentState(PaymentState newState)
{
    if (newState == m_paymentState)
        return;

    m_paymentState = newState;
    m_dirty = true;
    emit paymentStateChanged();
}

QString PaymentRequest::address() const
{
    if (m_address.IsNull())
        return QString();
    if (m_useLegacyAddressFormat) {
        CBase58Data legacy;
        legacy.setData(m_address, CBase58Data::PubkeyType,
                       FloweePay::instance()->chain() == P2PNet::MainChain
                       ? CBase58Data::Mainnet : CBase58Data::Testnet);
        return QString::fromStdString(legacy.ToString());
    }
    CashAddress::Content c;
    c.hash = std::vector<uint8_t>(m_address.begin(), m_address.end());
    c.type = CashAddress::PUBKEY_TYPE;
    return QString::fromStdString(CashAddress::encodeCashAddr(FloweePay::instance()->chainPrefix(), c));
}

void PaymentRequest::setWallet(Wallet *wallet)
{
    if (m_wallet == wallet)
        return;
    if (!m_unusedRequest && wallet != nullptr) {
        logFatal() << "Can't change a wallet on an already saved payment request";
        assert(m_unusedRequest);
        return;
    }

    if (m_wallet) {
        disconnect (m_wallet, SIGNAL(encryptionChanged()), this, SLOT(walletEncryptionChanged()));
        auto w = m_wallet;
        m_wallet = nullptr; // avoid recursion in the next line.
        w->removePaymentRequest(this);
        if (m_paymentState == Unpaid)
            w->unreserveAddress(m_privKeyId);
    }

    // if the wallet is encrypted we don't use it.
    m_wallet = wallet;
    if (m_wallet) {
        const bool closedWAllet = m_wallet->encryption() == Wallet::FullyEncrypted && !m_wallet->isDecrypted();
        if (closedWAllet) {
            // a closed wallet is barely a husk of a data-structure. So a payment request makes no sense for it.
            // but we can listen to the signal and after that initialize our request.
            m_address = KeyId();
        }
        else {
            m_privKeyId = m_wallet->reserveUnusedAddress(m_address);
            m_wallet->addPaymentRequest(this);
        }
        connect (m_wallet, SIGNAL(encryptionChanged()), this, SLOT(walletEncryptionChanged()));
    }
    emit walletChanged();
    emit qrCodeStringChanged();
}

void PaymentRequest::walletEncryptionChanged()
{
    assert (m_wallet);
    const bool closedWAllet = m_wallet->encryption() == Wallet::FullyEncrypted && !m_wallet->isDecrypted();
    if (closedWAllet) {
        m_address = KeyId();
    }
    else {
        m_privKeyId = m_wallet->reserveUnusedAddress(m_address);
        m_wallet->addPaymentRequest(this);
    }
    emit qrCodeStringChanged();
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
    if (m_wallet->isSingleAddressWallet()
            && m_unusedRequest && m_message.isEmpty()
            && m_amountRequested == 0) {
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
    m_dirty = true;

    if (m_paymentState == Unpaid && m_amountSeen >= m_amountRequested) {
        if (blockHeight == -1)  {
            setPaymentState(PaymentSeen);
            QTimer::singleShot(FloweePay::instance()->dspTimeout(), this, [=]() {
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

bool PaymentRequest::stored() const
{
    return !m_unusedRequest;
}

void PaymentRequest::setStored(bool on)
{
    if (on != m_unusedRequest)
        return;
    m_unusedRequest = !on;
    setParent(m_wallet);
    setSaveState(on ? Stored : Temporary);
    if (!on) {
        m_wallet->removePaymentRequest(this);
        // deleteLater(); // possible memory leak, but better than crashes
    }

    emit storedChanged();
}

PaymentRequest::SaveState PaymentRequest::saveState() const
{
    return m_saveState;
}

void PaymentRequest::setSaveState(SaveState saveState)
{
    if (m_saveState == saveState)
        return;
    m_saveState = saveState;
    m_dirty = true;
    emit saveStateChanged();
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

void PaymentRequest::switchAccount(AccountInfo *ai)
{
    assert(ai);
    setWallet(ai->wallet());
}
