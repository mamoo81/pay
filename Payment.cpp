/*
 * This file is part of the Flowee project
 * Copyright (C) 2020 Tom Zander <tom@flowee.org>
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
#include "AccountInfo.h"

#include <base58.h>
#include <cashaddr.h>
#include <TransactionBuilder.h>
#include <QTimer>

// #define DEBUG_TX_CREATION
#ifdef DEBUG_TX_CREATION
# include <QDir>
# include <QFile>
#endif

namespace {
bool validatePayment(const QList<PaymentDetail*> &details, bool &hasMaxOutput)
{
    int numOutputs = 0;
    for (auto detail : details) {
        if (detail->type() == Payment::PayToAddress) {
            ++numOutputs;
            auto o = detail->toOutput();
            if (o->amount() == -1) {
                if (hasMaxOutput) { // not the only one that is 'Max'
                    logFatal().nospace() << "Payment::prepare: Can not create transaction, "
                       "faulty payment amount[" << numOutputs << "] Multiple MAX outputs";
                    return false;
                }
                else {
                    hasMaxOutput = true;
                }
            }
            else if (o->amount() < 600) {
                logFatal().nospace() << "Payment::prepare: Can not create transaction, "
                   "faulty payment amount[" << numOutputs << "] " << o->amount();
                return false;
            }
            if (o->formattedTarget().isEmpty()) {
                logFatal() << "Payment::prepare: Can not create transaction, missing data for output:"
                           << numOutputs;
                return false;
            }
        }
    }

    return true;
}
}


// ///////////////////////////////////////////////

Payment::Payment(QObject *parent)
    : QObject(parent),
      m_fee(1)
{
    auto out = new PaymentDetailOutput(this);
    out->setCollapsable(false);
    m_paymentDetails.append(out);
    // the first one is special since its used to emulate a dumb payment API
    connect (out, SIGNAL(paymentAmountChanged()), this, SIGNAL(amountChanged()));
    connect (out, SIGNAL(addressChanged()), this, SIGNAL(targetAddressChanged()));
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

PaymentDetailOutput *Payment::soleOut() const
{
    if (m_paymentDetails.size() != 1) // don't mix modes.
        throw std::runtime_error("Don't mix payment modes, use the details");
    auto out = dynamic_cast<PaymentDetailOutput*>(m_paymentDetails.first());
    assert(out);
    return out;
}

void Payment::setPaymentAmount(double amount)
{
    soleOut()->setPaymentAmount(amount);
}

double Payment::paymentAmount()
{
    return soleOut()->paymentAmount();
}

void Payment::setTargetAddress(const QString &address)
{
    soleOut()->setAddress(address);
}

QString Payment::targetAddress()
{
    return soleOut()->address();
}

QString Payment::formattedTargetAddress()
{
    return soleOut()->formattedTarget();
}

bool Payment::validate()
{
    bool hasMaxOutput;
    return validatePayment(m_paymentDetails, hasMaxOutput);
}

void Payment::prepare(AccountInfo *account)
{
    if (account == nullptr || account->wallet() == nullptr)
        throw std::runtime_error("Required account missing");
    assert(account);
    assert(account->wallet());
    assert(account->wallet()->segment());
    m_wallet = account->wallet();
    bool seenMaxAmount = false; // set to true if the last output detail was set to 'Max'
    if (!validatePayment(m_paymentDetails, seenMaxAmount))
        return;

    TransactionBuilder builder;
    int numOutputs = 0;
    qint64 totalOut = 0;
    for (auto detail : m_paymentDetails) {
        if (detail->type() == PayToAddress) {
            ++numOutputs;
            auto o = detail->toOutput();
            totalOut += o->amount();

            builder.appendOutput(o->amount());

            bool ok = false;
            CashAddress::Content c = CashAddress::decodeCashAddrContent(o->formattedTarget().toStdString(), chainPrefix());
            assert(!c.hash.empty());
            if (c.type == CashAddress::PUBKEY_TYPE) {
                builder.pushOutputPay2Address(CKeyID(reinterpret_cast<const char*>(c.hash.data())));
                ok = true;
            }
            else if (c.type == CashAddress::SCRIPT_TYPE) {
                builder.pushOutputPay2Script(CScriptID(reinterpret_cast<const char*>(c.hash.data())));
                ok = true;
            }
            assert(ok); // mismatch between setPayTo and this method...
        }
    }

    auto tx = builder.createTransaction();

    // find and add outputs that can be used to pay for our required output
    int64_t change = -1;
    const auto funding = m_wallet->findInputsFor(totalOut, m_fee, tx.size(), change);
    if (funding.outputs.empty()) // not enough funds.
        return;

    qint64 fundsIngoing = 0;
    for (auto ref : funding.outputs) {
        builder.appendInput(m_wallet->txid(ref), ref.outputIndex());
        auto output = m_wallet->txOutput(ref);
        fundsIngoing += output.outputValue;
        auto priv = m_wallet->unlockKey(ref);
        if (priv.sigType == Wallet::NotUsedYet) {
            priv.sigType = m_preferSchnorr ? Wallet::SignedAsSchnorr : Wallet::SignedAsEcdsa;
            m_wallet->updateSignatureType(priv); // remember the signing type for next time.
        }
        TransactionBuilder::SignatureType typeToUse =
                (priv.sigType == Wallet::SignedAsEcdsa) ? TransactionBuilder::ECDSA : TransactionBuilder::Schnorr;
        builder.pushInputSignature(priv.key, output.outputScript, output.outputValue, typeToUse);
    }

    m_assignedFee = 0;
    int changeOutput = -1;
    if (seenMaxAmount) { // if we pay everything to the last output
        change = fundsIngoing;
        changeOutput = builder.outputCount() - 1;
        builder.selectOutput(changeOutput);
        builder.setOutputValue(change); // pays no fee yet
    } else {
        m_assignedFee = fundsIngoing - totalOut;

        // notice that the findInputsFor() will try really really hard to avoid us
        // having change greater than 100 and less than 1000.
        // But if we hit that, its better to give it to the miners than to
        // create a tiny change UTXO
        if (change > 1000) { // create a change-output
            changeOutput = builder.appendOutput(change);
            builder.pushOutputPay2Address(m_wallet->nextUnusedChangeAddress());
            m_assignedFee -= change;
        }
    }
    assert(m_assignedFee >= 0);
    m_tx = builder.createTransaction();

    // now double-check the fee since we can't predict the tx-size size perfectly.
    while (changeOutput != -1) {
        // a positive diff means we underpaid fee
        const int diff = m_tx.size() * m_fee - m_assignedFee;
        if (diff <= 0)
            break;
        change -= diff;
        m_assignedFee += diff;

        builder.selectOutput(changeOutput);
        builder.setOutputValue(change);
        m_tx = builder.createTransaction();
    }
    if (seenMaxAmount) {
        totalOut = fundsIngoing;
        // TODO remember this?
    }

    m_txPrepared = true;
    emit txCreated();
    emit txPreparedChanged();
#ifdef DEBUG_TX_CREATION
    QFile out(QDir::homePath() + "/flowee-pay-" + QString::fromStdString(m_tx.createHash().ToString()));
    if (out.open(QIODevice::WriteOnly)) {
        out.write(m_tx.data().begin(), m_tx.size());
        out.close();

        logCritical() << "Wrote tx to" << out.fileName();
    }
#endif
}

void Payment::broadcast()
{
    if (!m_txPrepared)
        return;

    /*
     * Wallet:
     *  * Make the AccountInfo able to show these unconfirmed transactions so we
     *    can cancel one to free up the utxos.
     */

    // call to wallet to mark outputs locked and save tx.
    m_wallet->newTransaction(m_tx);

    m_infoObject = std::make_shared<PaymentInfoObject>(this, m_tx);
    FloweePay::instance()->p2pNet()->connectionManager().broadcastTransaction(m_infoObject);
}

PaymentDetail* Payment::addDetail(PaymentDetail *detail)
{
    m_paymentDetails.append(detail);
    emit paymentDetailsChanged();
    return detail;
}

QList<QObject*> Payment::paymentDetails() const
{
    QList<QObject*> pds;
    for (auto *detail : m_paymentDetails) {
        pds.append(detail);
    }
    return pds;
}

PaymentDetail* Payment::addExtraOutput()
{
    return addDetail(new PaymentDetailOutput(this));
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

bool Payment::preferSchnorr() const
{
    return m_preferSchnorr;
}

void Payment::setPreferSchnorr(bool preferSchnorr)
{
    if (m_preferSchnorr == preferSchnorr)
        return;

    m_preferSchnorr = preferSchnorr;
    emit preferSchnorrChanged();
}

bool Payment::txPrepared() const
{
    return m_txPrepared;
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

PaymentDetail::PaymentDetail(Payment::DetailType type, QObject *parent)
    : QObject(parent),
      m_type(type)
{
}

Payment::DetailType PaymentDetail::type() const
{
    return m_type;
}

bool PaymentDetail::collapsable() const
{
    return m_collapsable;
}

void PaymentDetail::setCollapsable(bool newCollapsable)
{
    if (m_collapsable == newCollapsable)
        return;
    m_collapsable = newCollapsable;
    emit collapsableChanged();
}

bool PaymentDetail::collapsed() const
{
    return m_collapsed;
}

void PaymentDetail::setCollapsed(bool newCollapsed)
{
    if (m_collapsed == newCollapsed)
        return;
    m_collapsed = newCollapsed;
    emit collapsedChanged();
}


// ///////////////////////////////////////////////

PaymentDetailInputs::PaymentDetailInputs(QObject *parent)
    : PaymentDetail(Payment::InputSelector, parent)
{
}


// ///////////////////////////////////////////////

PaymentDetailOutput::PaymentDetailOutput(QObject *parent)
    : PaymentDetail(Payment::PayToAddress, parent)
{

}

double PaymentDetailOutput::paymentAmount() const
{
    return static_cast<double>(m_paymentAmount);
}

void PaymentDetailOutput::setPaymentAmount(double newPaymentAmount)
{
    qint64 amount = static_cast<qint64>(newPaymentAmount);
    if (m_paymentAmount == amount)
        return;
    m_paymentAmount = amount;
    emit paymentAmountChanged();
}

const QString &PaymentDetailOutput::address() const
{
    return m_address;
}

void PaymentDetailOutput::setAddress(const QString &address_)
{
    const QString address = address_.trimmed();
    if (m_address == address)
        return;

    switch (FloweePay::instance()->identifyString(address)) {
    case FloweePay::LegacyPKH: {
        CBase58Data legacy;
        auto ok = legacy.SetString(m_address.toStdString());
        assert(ok);
        assert(legacy.isMainnetPkh() || legacy.isTestnetPkh());
        CashAddress::Content c;
        c.hash = legacy.data();
        c.type = CashAddress::PUBKEY_TYPE;
        m_formattedTarget = QString::fromStdString(CashAddress::encodeCashAddr(chainPrefix(), c));
        break;
    }
    case FloweePay::CashPKH: {
        auto c = CashAddress::decodeCashAddrContent(m_address.toStdString(), chainPrefix());
        assert (!c.hash.empty() && c.type == CashAddress::PUBKEY_TYPE);
        m_formattedTarget = QString::fromStdString(CashAddress::encodeCashAddr(chainPrefix(), c));
        break;
    }
    case FloweePay::LegacySH: {
        CBase58Data legacy;
        auto ok = legacy.SetString(m_address.toStdString());
        assert(ok);
        assert(legacy.isMainnetSh() || legacy.isTestnetSh());
        CashAddress::Content c;
        c.hash = legacy.data();
        c.type = CashAddress::SCRIPT_TYPE;
        m_formattedTarget = QString::fromStdString(CashAddress::encodeCashAddr(chainPrefix(), c));
        break;
    }
    case FloweePay::CashSH: {
        auto c = CashAddress::decodeCashAddrContent(m_address.toStdString(), chainPrefix());
        assert (!c.hash.empty() && c.type == CashAddress::SCRIPT_TYPE);
        m_formattedTarget = QString::fromStdString(CashAddress::encodeCashAddr(chainPrefix(), c));
        break;
    }
    default:
        m_formattedTarget.clear();
        return;
    }

    m_address = address;
    emit addressChanged();
}

const QString &PaymentDetailOutput::formattedTarget() const
{
    return m_formattedTarget;
}
