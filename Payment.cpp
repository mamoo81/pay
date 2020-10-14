/*
 * This file is part of the Flowee project
 * Copyright (C) 2020 Tom Zander <tomz@freedommail.ch>
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

#include "Payment.h"
#include "Payment_p.h"
#include "FloweePay.h"
#include "Wallet.h"

#include <base58.h>
#include <cashaddr.h>
#include <TransactionBuilder.h>
#include <QTimer>

Payment::Payment(Wallet *wallet, qint64 amountToPay)
    : m_wallet(wallet),
      m_fee(1),
      m_paymentAmount(amountToPay)
{
    assert(m_wallet);
    assert(m_wallet->segment());
}

void Payment::setFeePerByte(int sats)
{
    if (m_fee == sats)
        return;
    m_fee = sats;
    emit feePerByteChanged();
}

int Payment::feePerByte()
{
    return m_fee;
}

void Payment::setPaymentAmount(double amount_)
{
    qint64 amount = static_cast<qint64>(amount_);
    if (m_paymentAmount == amount)
        return;
    m_paymentAmount = amount;
    emit amountChanged();
}

double Payment::paymentAmount()
{
    return static_cast<double>(m_paymentAmount);
}

void Payment::setTargetAddress(const QString &address)
{
    if (m_address == address)
        return;
    switch (FloweePay::instance()->identifyString(address)) {
    case FloweePay::LegacyPKH: {
        m_address = address.trimmed();
        CBase58Data legacy;
        auto ok = legacy.SetString(m_address.toStdString());
        assert(ok);
        assert(legacy.isMainnetPkh());
        CashAddress::Content c;
        c.hash = legacy.data();
        c.type = CashAddress::PUBKEY_TYPE;
        m_formattedTarget = QString::fromStdString(CashAddress::encodeCashAddr("bitcoincash", c));

        emit targetAddressChanged();
        break;
    }
    case FloweePay::CashPKH: {
        m_address = address.trimmed();
        auto c = CashAddress::decodeCashAddrContent(m_address.toStdString(), "bitcoincash");
        assert (!c.hash.empty() && c.type == CashAddress::PUBKEY_TYPE);
        m_formattedTarget = QString::fromStdString(CashAddress::encodeCashAddr("bitcoincash", c));
        emit targetAddressChanged();
        break;
    }
    case FloweePay::CashSH:
    case FloweePay::LegacySH:
        throw std::runtime_error("Unsupported at this time");
    default:
        throw std::runtime_error("Address not recognized");
    }
}

QString Payment::targetAddress()
{
    return m_address;
}

QString Payment::formattedTargetAddress()
{
    return m_formattedTarget;
}

void Payment::approveAndSign()
{
    if (m_formattedTarget.isEmpty() || m_paymentAmount < 600)
        throw std::runtime_error("Can not create transaction, missing data");
    TransactionBuilder builder;
    builder.appendOutput(m_paymentAmount);
    bool ok = false;
    CashAddress::Content c = CashAddress::decodeCashAddrContent(m_formattedTarget.toStdString(), "bitcoincash");
    assert(!c.hash.empty());
    if (c.type == CashAddress::PUBKEY_TYPE) {
        builder.pushOutputPay2Address(CKeyID(reinterpret_cast<const char*>(c.hash.data())));
        ok = true;
    }
    // else if (c.type == CashAddress::SCRIPT_TYPE)
    // TODO p2sh must be added in transactionbuilder first
    assert(ok); // mismatch between setPayTo and this method...

    auto tx = builder.createTransaction();

    // find and add outputs that can be used to pay for our required output
    int64_t change = -1;
    const auto prevOuts = m_wallet->findInputsFor(m_paymentAmount, m_fee, tx.size(), change);
    if (prevOuts.empty())
        throw std::runtime_error("Not enough funds");
    m_assignedFee = 0;
    qint64 fundsIngoing = 0;
    for (auto ref : prevOuts) {
        builder.appendInput(m_wallet->txid(ref), ref.outputIndex());
        auto output = m_wallet->txOutout(ref);
        fundsIngoing += output.outputValue;
        builder.pushInputSignature(m_wallet->unlockKey(ref), output.outputScript, output.outputValue);
    }

    m_assignedFee = fundsIngoing - m_paymentAmount;
    int changeOutput = -1;
    if (change > 1000) {
        // notice that the findInputsFor() will try really really hard to avoid us
        // having change greater than 100 and less than 1000.
        // But if we hit that, its better to give it to the miners than to
        // create a tiny change UTXO
        changeOutput = builder.appendOutput(change);
        builder.pushOutputPay2Address(m_wallet->nextChangeAddress());
        m_assignedFee -= change;
    }
    m_tx = builder.createTransaction();

    // now double-check the fee since we can't predict the signature size perfectly.
    int diff = m_tx.size() * m_fee - m_assignedFee;
    if (diff != 0 && changeOutput != -1) {
        // a positive diff means we underpaid fee
        builder.selectOutput(changeOutput);
        builder.setOutputValue(change - diff);
        m_assignedFee += diff;
        m_tx = builder.createTransaction();
    }
    emit txCreated();
}

void Payment::sendTx()
{
    /*
     * TODO
     *  - call to wallet to mark outputs locked and save tx.
     */

    /*
     * Wallet:
     *  * Add unspent-output as 'locked' when its spent but not confirmed.
     *  * Have a new map for transactions we created (with outputs that are now locked)
     *    and the blockheight we actually created it at.
     *  * Make the AccountInfo able to show these unconfirmed transactions so we
     *    can cancel one to free up the utxos.
     *  * make sure that after a block came in we remove any transactions in that list
     *    which can no longer confirm.
     */

    m_infoObject = std::make_shared<PaymentInfoObject>(this, m_tx);
    FloweePay::instance()->p2pNet()->connectionManager().broadcastTransaction(m_infoObject);
}

QString Payment::txid() const
{
    if (!m_tx.isValid())
        return QString();
    return QString::fromStdString(m_tx.createHash().ToString());
}

Wallet *Payment::wallet() const
{
    return m_wallet;
}

void Payment::sentToPeer()
{
    // this callback happens when one of our peers did a getdata for the transaction.
    emit sent(++m_sentPeerCount);

    if (m_sentPeerCount >= 2) {
        // if two peers requested the tx, then wait a bit and check on the status
        QTimer::singleShot(5 * 1000, [=]() {
            if (m_sentPeerCount - m_rejectedPeerCount > 2) {
                // When enough peers received the transaction stop broadcasting it.
                m_infoObject.reset();
            }
        });
    }
}

void Payment::txRejected(short reason, const QString &message)
{
    // reason is hinted using BroadcastTxData::RejectReason
    logCritical() << "Transaction rejected" << reason << message;
    ++m_rejectedPeerCount;
}

int Payment::assignedFee() const
{
    return m_assignedFee;
}

int Payment::txSize() const
{
    return m_tx.size();
}


// //////////////////////////////////////////////////

PaymentInfoObject::PaymentInfoObject(Payment *payment, const Tx &tx)
    : BroadcastTxData(tx),
      m_parent(payment)
{
    connect(this, SIGNAL(sentOneFired()), payment, SLOT(sentToPeer()), Qt::QueuedConnection);
    connect(this, SIGNAL(txRejectedFired(short,QString)), payment,
            SLOT(txRejected(short,QString)), Qt::QueuedConnection);
}

void PaymentInfoObject::txRejected(RejectReason reason, const std::string &message)
{
    emit txRejectedFired(reason, QString::fromStdString(message));
}

void PaymentInfoObject::sentOne()
{
    emit sentOneFired();
}

uint16_t PaymentInfoObject::privSegment() const
{
    assert(m_parent);
    assert(m_parent->wallet());
    assert(m_parent->wallet()->segment());
    return m_parent->wallet()->segment()->segmentId();
}
