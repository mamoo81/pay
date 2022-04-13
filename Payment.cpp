/*
 * This file is part of the Flowee project
 * Copyright (C) 2020-2022 Tom Zander <tom@flowee.org>
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
#include "PaymentDetailInputs_p.h"
#include "PaymentDetailOutput_p.h"
#include "FloweePay.h"
#include "AccountInfo.h"

#include <cashaddr.h>
#include <TransactionBuilder.h>
#include <QTimer>

// #define DEBUG_TX_CREATION
#ifdef DEBUG_TX_CREATION
# include <QDir>
# include <QFile>
#endif


Payment::Payment(QObject *parent)
    : QObject(parent)
{
    reset();
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
    int64_t sats = 0;
    for (auto d : m_paymentDetails) {
        if (d->isOutput()) {
            auto o = d->toOutput();
            if (!(o->maxAllowed() && o->maxSelected()))
                sats += o->paymentAmount();
        }
    }
    return static_cast<double>(sats);
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
    int64_t output = 0;
    for (auto detail : m_paymentDetails) {
        if (!detail->valid())
            return false;
        if (detail->isOutput())
            output += detail->toOutput()->paymentAmount();
        if (detail->isInputs()) {
            // we are ensured that all outputs come before the input-selector.
            // Check that the selected inputs contain more than is output
            if (output > detail->toInputs()->selectedValue()) {
                logDebug() << "Need more inputs to cover the requested output-amounts";
                return false;
            }
        }
    }
    return output > 547;
}

void Payment::prepare()
{
    if (m_account == nullptr || m_account->wallet() == nullptr)
        throw std::runtime_error("Required account missing");
    assert(m_account);
    assert(m_account->wallet());
    assert(m_account->wallet()->segment());
    if (!validate())
        throw std::runtime_error("can't prepare an invalid Payment");
    m_wallet = m_account->wallet();

    TransactionBuilder builder;
    qint64 totalOut = 0;
    bool seenMaxAmount = false; // set to true if the last output detail was set to 'Max'
    PaymentDetailInputs *inputs = nullptr;
    for (auto detail : m_paymentDetails) {
        if (detail->type() == PayToAddress) {
            assert(!seenMaxAmount); // only allowed once.
            auto o = detail->toOutput();
            seenMaxAmount |= o->maxAllowed() && o->maxSelected();
            if (!seenMaxAmount)
                totalOut += o->paymentAmount();
            builder.appendOutput(o->paymentAmount());

            bool ok = false;
            auto address = o->formattedTarget();
            if (address.isEmpty())
                address = o->address();
            assert(!address.isEmpty());
            CashAddress::Content c = CashAddress::decodeCashAddrContent(address.toStdString(), chainPrefix());
            assert(!c.hash.empty());
            if (c.type == CashAddress::PUBKEY_TYPE) {
                builder.pushOutputPay2Address(CKeyID(reinterpret_cast<const char*>(c.hash.data())));
                ok = true;
            }
            else if (c.type == CashAddress::SCRIPT_TYPE) {
                builder.pushOutputPay2Script(CScriptID(reinterpret_cast<const char*>(c.hash.data())));
                ok = true;
            }
            assert(ok); // mismatch between PaymentDetailOutput::setAddress and this method...
        }
        else if (detail->type() == InputSelector) {
            inputs = detail->toInputs();
        }
    }

    auto tx = builder.createTransaction();

    // find and add outputs that can be used to pay for our required output
    int64_t change = -1;
    Wallet::OutputSet funding;
    if (inputs) {
        funding = inputs->selectedInputs(seenMaxAmount ? -1 : totalOut, m_fee, tx.size(), change);
        if (funding.outputs.empty()) { // not enough funds.
            // This can only be due to fees as we called 'verify' above.
            m_error = tr("Not enough funds selected for fees");
            emit errorChanged();
            return;
        }
    }
    else {
        funding = m_wallet->findInputsFor(seenMaxAmount ? -1 : totalOut, m_fee, tx.size(), change);
        if (funding.outputs.empty()) { // not enough funds.
            m_error = tr("Not enough funds in wallet to make payment!");
            emit errorChanged();
            return;
        }
    }
    if (!m_error.isEmpty()) {
        m_error.clear();
        emit errorChanged();
    }

    for (auto ref : funding.outputs) {
        builder.appendInput(m_wallet->txid(ref), ref.outputIndex());
        auto output = m_wallet->txOutput(ref);
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
    if (seenMaxAmount) { // if we pay the rest to the last output
        change = funding.totalSats - totalOut;
        changeOutput = builder.outputCount() - 1;
        builder.selectOutput(changeOutput);
        builder.setOutputValue(change); // pays no fee yet
    } else {
        m_assignedFee = funding.totalSats - totalOut;

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

    if (m_tx.size() > 100000) { // max size of a transaction is 100KB
        m_error = tr("Transaction too large. Amount selected needs too many coins.");
        emit errorChanged();
        return;
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

    // call to wallet to mark outputs locked and save tx.
    m_wallet->newTransaction(m_tx);
    if (!m_userComment.isEmpty())
        m_wallet->setTransactionComment(m_tx, m_userComment);

    m_infoObject = std::make_shared<PaymentInfoObject>(this, m_tx);
    FloweePay::instance()->p2pNet()->connectionManager().broadcastTransaction(m_infoObject);
    m_txBroadcastStarted = true;
    emit broadcastStatusChanged();
}

void Payment::reset()
{
    m_fiatPrice = 50000; // just have it initialized
    m_fee = 1;
    m_txPrepared = false;
    m_txBroadcastStarted = false;
    m_preferSchnorr = true;
    m_error.clear();
    m_tx = Tx();
    m_assignedFee = 0;
    m_infoObject.reset();
    m_sentPeerCount = 0;
    m_rejectedPeerCount = 0;
    m_userComment.clear();

    for (auto d : m_paymentDetails) {
        d->deleteLater();
    }
    m_paymentDetails.clear();
    emit paymentDetailsChanged(); // Instantly update UI
    auto out = new PaymentDetailOutput(this);
    out->setCollapsable(false);
    // the first one is special since its used to emulate a dumb payment API
    connect (out, SIGNAL(paymentAmountChanged()), this, SIGNAL(amountChanged()));
    connect (out, SIGNAL(addressChanged()), this, SIGNAL(targetAddressChanged()));
    addDetail(out);

    emit feePerByteChanged();
    emit amountChanged();
    emit targetAddressChanged();
    emit txPreparedChanged();
    emit preferSchnorrChanged();
    emit broadcastStatusChanged();
    emit errorChanged();
    emit txCreated();
    emit userCommentChanged();
}

void Payment::addDetail(PaymentDetail *detail)
{
    if (m_paymentDetails.size() == 1) // Our initial one is made non-collapsable, revert that.
        m_paymentDetails.first()->setCollapsable(true);
    assert(detail);
    if (detail->isOutput()) {
        // outputs are grouped together at the beginning. We insert
        // at the end of the outputs, before any others.
        int newPos = 0;
        while (newPos < m_paymentDetails.length() && m_paymentDetails.at(newPos)->isOutput())
            ++newPos;
        m_paymentDetails.insert(newPos, detail);
    }
    else {
        m_paymentDetails.append(detail);
    }
    connect (detail, SIGNAL(validChanged()), this, SIGNAL(validChanged()));
    if (detail->isOutput()) {
        connect (detail, SIGNAL(paymentAmountChanged()), this, SLOT(recalcAmounts()));
        connect (detail, SIGNAL(fiatAmountChanged()), this, SLOT(recalcAmounts()));
    }
    if (detail->isInputs()) {
        connect (detail, SIGNAL(selectedValueChanged()), this, SLOT(recalcAmounts()));
    }
    assert(!m_paymentDetails.isEmpty());
    emit paymentDetailsChanged();
    emit validChanged(); // pretty sure we are invalid after ;-)
}

const QString &Payment::userComment() const
{
    return m_userComment;
}

void Payment::setUserComment(const QString &comment)
{
    if (m_userComment == comment)
        return;
    m_userComment = comment;
    if (m_txPrepared)
        m_wallet->setTransactionComment(m_tx, comment);
    emit userCommentChanged();
}

const QString &Payment::error() const
{
    return m_error;
}

AccountInfo *Payment::currentAccount() const
{
    return m_account;
}

void Payment::setCurrentAccount(AccountInfo *account)
{
    if (m_account == account)
        return;
    m_account = account;
    emit currentAccountChanged();

    for (auto detail : m_paymentDetails) {
        detail->setWallet(account->wallet());
    }
}

int Payment::fiatPrice() const
{
    return m_fiatPrice;
}

void Payment::setFiatPrice(int pricePerCoin)
{
    if (m_fiatPrice == pricePerCoin)
        return;
    m_fiatPrice = pricePerCoin;
    emit fiatPriceChanged();
}

QList<QObject*> Payment::paymentDetails() const
{
    QList<QObject*> pds;
    for (auto *detail : m_paymentDetails) {
        pds.append(detail);
    }
    return pds;
}

Payment::BroadcastStatus Payment::broadcastStatus() const
{
    if (!m_txBroadcastStarted)
        return NotStarted;
    if (m_sentPeerCount == 0)
        return TxOffered;
    if (m_rejectedPeerCount - m_sentPeerCount >= 0)
        return TxRejected;
    if (m_infoObject.get() == nullptr)
        return TxBroadcastSuccess;
    return TxWaiting;
}

void Payment::addExtraOutput()
{
    // only the last in the sequence can have 'max'
    for (auto d : m_paymentDetails) {
        if (d->isOutput())
            d->toOutput()->setMaxAllowed(false);
    }
    addDetail(new PaymentDetailOutput(this));
}

void Payment::addInputSelector()
{
    // only one input selector allowed
    for (auto d : m_paymentDetails) {
        if (d->isInputs()) {
            // un-collapse, but not add
            d->setCollapsed(false);
            return;
        }
    }
    addDetail(new PaymentDetailInputs(this));
}

void Payment::remove(PaymentDetail *detail)
{
    const auto count = m_paymentDetails.removeAll(detail);
    if (count) {
        emit paymentDetailsChanged();
        emit validChanged();

        // Make sure only the last output has 'maxAllowed' set to true.
        if (detail->isOutput()) {
            bool seenOne = false;
            for (auto iter = m_paymentDetails.rbegin(); iter != m_paymentDetails.rend(); ++iter) {
                auto *detail = *iter;
                if (detail->isOutput()) {
                    detail->toOutput()->setMaxAllowed(!seenOne);
                    seenOne = true;
                }
            }
        }
    }
    detail->deleteLater();
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

// this callback happens when one of our peers did a getdata for the transaction.
void Payment::sentToPeer()
{
    /*
     * We offered the Tx to all peers (typically 3) and we need at least one to have
     * downloaded it from us.
     * Possibly more peers will download it from us, but it is more likely that they
     * download it from each other and we won't be able to see progress.
     *
     * On the other hand, when a transaction is invalid we will get a rejection message
     * and then likely other nodes will download it from us as well and we'll get more
     * rejections that way.  For this reason we need to wait a seconds after download
     * started to see if we have more peers we sent it to than that rejected it.
     */
    ++m_sentPeerCount;

    QTimer::singleShot(1000, [=]() {
        if (m_rejectedPeerCount == 0 || m_sentPeerCount > 1) {
            // When enough peers received the transaction stop broadcasting it.
            m_infoObject.reset();
        }
        emit broadcastStatusChanged();
    });
    emit broadcastStatusChanged();
}

void Payment::txRejected(short reason, const QString &message)
{
    // reason is hinted using BroadcastTxData::RejectReason
    logCritical() << "Transaction rejected" << reason << message;
    ++m_rejectedPeerCount;
    emit broadcastStatusChanged();
}

void Payment::recalcAmounts()
{
    // Find the output that has 'max' and give it a change to recalculate the effective values.
    auto out = qobject_cast<PaymentDetailOutput*>(sender());
    if (out && out->maxSelected() && out->maxAllowed())
        return;
    for (auto i = m_paymentDetails.rbegin(); i != m_paymentDetails.rend(); ++i) {
        auto *detail = *i;
        if (detail->isOutput()) {
            if (detail != sender())
                detail->toOutput()->recalcMax();
            break; // only the last detail can be a 'max'
        }
    }
    emit amountChanged(); // also trigger the payment-wide paymentAmount property to have been changed
    emit validChanged();
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

int Payment::effectiveFiatAmount() const
{
    int amount = 0;
    PaymentDetailInputs *inputSelector = nullptr;
    for (auto i = m_paymentDetails.crbegin(); i != m_paymentDetails.crend(); ++i) {
        auto *detail = *i;
        if (detail->isInputs())
            inputSelector = detail->toInputs();
        if (!detail->isOutput())
            continue;
        auto *out = detail->toOutput();
        if (out->maxAllowed() && out->maxSelected()) {
            if (m_fiatPrice == 0)
                return 0;
            int64_t totalBch = 0;
            if (inputSelector) {
                totalBch = inputSelector->selectedValue();
            } else {
                // then the total amount is actually trivial, it is all that is available in the wallet.
                totalBch = m_wallet->balanceConfirmed() + m_wallet->balanceUnconfirmed();
            }

            return (totalBch * m_fiatPrice / 10000000 + 5) / 10;
        }
        amount += out->fiatAmount();
    }
    return amount;
}

double Payment::effectiveBchAmount() const
{
    int64_t amount = 0;
    PaymentDetailInputs *inputSelector = nullptr;
    for (auto i = m_paymentDetails.crbegin(); i != m_paymentDetails.crend(); ++i) {
        auto *detail = *i;
        if (detail->isInputs())
            inputSelector = detail->toInputs();
        if (!detail->isOutput())
            continue;
        auto *out = detail->toOutput();
        if (out->maxAllowed() && out->maxSelected()) {
            if (inputSelector)
                return inputSelector->selectedValue();
            // then the total amount is actually trivial, it is all that is available in the wallet.
            return m_wallet->balanceConfirmed() + m_wallet->balanceUnconfirmed();
        }
        amount += out->paymentAmount();
    }
    return static_cast<double>(amount);
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


// ///////////////////////////////

PaymentDetail::PaymentDetail(Payment *parent, Payment::DetailType type)
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

void PaymentDetail::setValid(bool valid)
{
    if (m_valid == valid)
        return;
    m_valid = valid;
    emit validChanged();
}

bool PaymentDetail::valid() const
{
    return m_valid;
}

void PaymentDetail::setWallet(Wallet *)
{
}

