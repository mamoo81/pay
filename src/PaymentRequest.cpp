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
#include "AccountInfo.h"
#include "FloweePay.h"
#include "Wallet.h"
#include "WalletKeyView.h"

#include <QTimer>
#include <QThread>
#include <QUrl>
#include <base58.h>
#include <cashaddr.h>

PaymentRequest::PaymentRequest(QObject *parent)
    : QObject(parent)
{
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

void PaymentRequest::updateFailReason()
{
    if (m_account == nullptr) {
        setFailReason(NoAccountSet);
    }
    else if (m_account->wallet()->encryption() == Wallet::FullyEncrypted
            && !m_account->wallet()->isDecrypted()) {
        setFailReason(AccountEncrypted);
    }
    else if (m_account->wallet()->walletIsImporting()) {
        setFailReason(AccountImporting);
    } else {
        setFailReason(NoFailure);
    }
}

PaymentRequest::FailReason PaymentRequest::failReason() const
{
    return m_failReason;
}

void PaymentRequest::setFailReason(FailReason reason)
{
    if (m_failReason == reason)
        return;
    m_failReason = reason;
    emit failReasonChanged();
    emit qrCodeStringChanged();
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

QString PaymentRequest::addressShort() const
{
    auto a = address();
    auto i = a.indexOf(':');
    if (i > 0)
        return a.mid(i + 1);
    return a;
}

QObject *PaymentRequest::account() const
{
    return m_account;
}

void PaymentRequest::setAccount(QObject *account)
{
    if (account == m_account)
        return;
    if (m_view && !m_view->transactions().isEmpty())
        throw std::runtime_error("Not allowed to change wallet on partially fulfilled request");

    if (m_account && m_privKeyId != -1 && m_paymentState == Unpaid)
        m_account->wallet()->unreserveAddress(m_privKeyId);

    delete m_view;
    m_view = nullptr;
    m_account = qobject_cast<AccountInfo*>(account);
    m_privKeyId = -1;
    m_address = KeyId();
    m_amountSeen = 0;
    if (m_account) {
        assert(QThread::currentThread() == thread());
        m_view = new WalletKeyView(m_account->wallet(), this);
        connect (m_view, &WalletKeyView::walletEncrypted, this, [=]() {
            updateFailReason();
        });
        connect (m_view, &WalletKeyView::importFinished, this, [=]() {
            updateFailReason();
            start();
        });
        connect (m_view, &WalletKeyView::transactionMatch, this, [=]() {
            uint64_t seen = 0;
            for (const auto &tx : m_view->transactions()) {
                if (tx.state != WalletKeyView::UTXORejected) {
                    seen += tx.amount;
                }
                // copy the user typed payment request to the wallet
                // for paying transactions.
                if (!m_message.isEmpty()) {
                    Wallet::OutputRef ref(tx.ref);
                    m_account->wallet()->setTransactionComment(ref.txIndex(), m_message);
                }
            }

            if (seen != m_amountSeen) {
                m_amountSeen = seen;
                if (m_amountSeen >= m_amountRequested) {
                    if (m_paymentState <= PartiallyPaid)
                        setPaymentState(PaymentSeen);
                }
                else if (m_amountSeen >= 0) {
                    if (m_paymentState == Unpaid)
                        setPaymentState(PartiallyPaid);
                }

                emit amountSeenChanged();
            }
        });
    }
    updateFailReason();
    emit accountChanged();
    emit qrCodeStringChanged();
    emit amountSeenChanged();
    emit addressChanged();
}

PaymentRequest::PaymentState PaymentRequest::paymentState() const
{
    return m_paymentState;
}

void PaymentRequest::start()
{
    if (m_failReason == NoFailure && m_privKeyId == -1) {
        m_privKeyId = m_account->wallet()->reserveUnusedAddress(m_address, Wallet::ReceivePath);
        assert(m_view);
        m_view->setPrivKeyIndex(m_privKeyId);
        emit qrCodeStringChanged();
        emit addressChanged();

#if 0
        // by enabling this you can simulate the payment request being fulfilled
        QTimer::singleShot(5000, [=]() {
            setPaymentState(PaymentSeen);
            QTimer::singleShot(3000, [=]() {
                setPaymentState(PaymentSeenOk);
                // setPaymentState(DoubleSpentSeen);
            });
        });
#endif
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
    if (m_failReason != NoFailure || m_address.IsNull()) {
        return QString();
    }
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
    m_message.clear();
    emit messageChanged();
    m_address = KeyId();
    m_amountRequested = 0;
    emit amountChanged();
    m_amountSeen = 0;
    setPaymentState(Unpaid);
    delete m_view;
    m_view = nullptr;
    setAccount(nullptr);
    assert(m_view == nullptr); // ensure we didn't get a new one
    m_privKeyId = -1;
}
