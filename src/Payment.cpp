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

#include "Payment.h"
#include "Payment_p.h"
#include "PaymentDetailInputs_p.h"
#include "PaymentDetailOutput_p.h"
#include "FloweePay.h"
#include "AccountInfo.h"
#include "PriceDataProvider.h"
#include "AccountConfig.h"

#include <cashaddr.h>
#include <TransactionBuilder.h>
#include <QTimer>
#include <QUrlQuery>

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
    doAutoPrepare();
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
    doAutoPrepare();
}

void Payment::setPaymentAmountFiat(int amount)
{
    soleOut()->setPaymentAmountFiat(amount);
    doAutoPrepare();
}

void Payment::setTargetAddress(const QString &address)
{
    soleOut()->setAddress(address);
    emit targetAddressChanged();
    doAutoPrepare();
}

QString Payment::targetAddress()
{
    return soleOut()->address();
}

QString Payment::formattedTargetAddress()
{
    return soleOut()->formattedTarget();
}

bool Payment::walletNeedsPin() const
{
    if (!m_account)
        return false;
    if (m_account->wallet()->encryption() == Wallet::NotEncrypted)
        return false;
    const auto &secrets = m_account->wallet()->walletSecrets();
    for (auto i = secrets.begin(); i != secrets.end(); ++i) {
        if (i->second.privKey.isValid())
            return false;
    }
    return true;
}

void Payment::decrypt(const QString &password)
{
    assert(m_account);
    if (!m_error.isEmpty()) {
        m_error.clear();
        emit errorChanged();
    }
    if (!m_account->decrypt(password)) {
        m_error = tr("Invalid PIN");
        emit errorChanged();
    }
    doAutoPrepare();
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
    if (m_wallet->encryption() > Wallet::NotEncrypted) {
        if (!m_wallet->isDecrypted())
            throw std::runtime_error("Wallet is needs to be decrypted first");
    }

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
                builder.pushOutputPay2Address(KeyId(reinterpret_cast<const char*>(c.hash.data())));
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
            m_txPrepared = false;
            emit txPreparedChanged();
            return;
        }
    }
    else {
        funding = m_wallet->findInputsFor(seenMaxAmount ? -1 : totalOut, m_fee, tx.size(), change);
        if (funding.outputs.empty()) { // not enough funds.
            m_error = tr("Not enough funds in wallet to make payment!");
            emit errorChanged();
            m_txPrepared = false;
            emit txPreparedChanged();
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
        assert(priv.key.isValid());
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
    builder.setAnonimize(true); // sort created tx' inputs / outputs alphabetically
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

    if (m_allowInstaPay) {
        PaymentDetailOutput *out = nullptr;
        if (m_paymentDetails.length() == 1) {
            // insta-pay is used to fund a single output, anything more complex than that
            // should be presented to the user for approval, no exceptions.
            out = m_paymentDetails.first()->toOutput();
        }
        if (!out)
            return;
        if (out->maxSelected())
            return;

        auto *fp = FloweePay::instance();
        auto *prices = fp->prices();
        assert(prices);
        auto currency = prices->currencyName();
        assert(!currency.isEmpty());
        AccountConfig conf(m_wallet);
        if (!conf.allowInstaPay()) {
            logInfo() << "Payment-prepare. Insta-pay for wallet is turned off:" << m_wallet->name();
            return;
        }
        auto limit = conf.fiatInstaPayLimit(currency);
        logInfo() << "Payment-prepare. Insta-pay for" << currency << "set to" << limit << "payment is" << out->paymentAmountFiat();
        if (out->paymentAmountFiat() > limit)
            return;

        // schedule broadcast in a different event in order to
        // allow multiple changes and prepare()s to happen and
        // only send the best version to the network.
        QTimer::singleShot(50, this, SLOT(broadcast()));
    }
}

void Payment::broadcast()
{
    if (!m_txPrepared || m_txBroadcastStarted)
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
    m_wallet = nullptr;

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
    if (detail->isOutput())
        connect (detail, SIGNAL(paymentAmountChanged()), this, SLOT(recalcAmounts()));
    if (detail->isInputs())
        connect (detail, SIGNAL(selectedValueChanged()), this, SLOT(recalcAmounts()));
    assert(!m_paymentDetails.isEmpty());
    emit paymentDetailsChanged();
    emit validChanged(); // pretty sure we are invalid after ;-)
    doAutoPrepare();
}

bool Payment::allowInstaPay() const
{
    return m_allowInstaPay;
}

void Payment::setAllowInstaPay(bool newAllowInstaPay)
{
    if (m_allowInstaPay == newAllowInstaPay)
        return;
    m_allowInstaPay = newAllowInstaPay;
    emit allowInstaPayChanged();

    /*
     * InstaPay is typically enabled together with auto-prepare and that gives
     * great UX for, well, instantly paying.
     * BUT, if the prepare() failed, we should stop trying to do the 'instaPay'.
     * It failed, now let the user decide when to send.
     */
    QTimer::singleShot(10, this, [=]() {
        if (!m_error.isEmpty())
            m_allowInstaPay = false;
    });
}

bool Payment::autoPrepare() const
{
    return m_autoPrepare;
}

void Payment::setAutoPrepare(bool newAutoPrepare)
{
    if (m_autoPrepare == newAutoPrepare)
        return;
    m_autoPrepare = newAutoPrepare;
    emit autoPrepareChanged();
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
    emit walletPinChanged();

    for (auto detail : m_paymentDetails) {
        detail->setWallet(account->wallet());
    }
    doAutoPrepare();
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
    doAutoPrepare();
}

QList<QObject*> Payment::paymentDetails() const
{
    QList<QObject*> pds;
    for (auto *const detail : m_paymentDetails) {
        pds.append(detail);
    }
    return pds;
}

Payment::BroadcastStatus Payment::broadcastStatus() const
{
#if 0
    // This default-disabled code-snippet is fun to allow developing the UX/GUI by stepping through the steps.
    // Alter the 'i == 4' to another value to make it stop at the step you want to see longer.
    static int i = 0;
    static QTimer *timer = nullptr;
    if (timer == nullptr) {
        timer = new QTimer(const_cast<Payment*>(this));
        timer->start(3000);
        connect(timer, &QTimer::timeout, [=]() {
            if (++i == 4)
                timer->stop();
            emit const_cast<Payment*>(this)->broadcastStatusChanged();
        });
    }
    switch (i) {
        case 0: return NotStarted;
        case 1: return TxOffered;
        case 3: return TxRejected;
        case 4: return TxBroadcastSuccess;;
        default: return TxWaiting;
    }
#endif
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

PaymentDetail *Payment::addExtraOutput()
{
    // only the last in the sequence can have 'max'
    for (auto d : m_paymentDetails) {
        if (d->isOutput())
            d->toOutput()->setMaxAllowed(false);
    }
    auto detail = new PaymentDetailOutput(this);
    addDetail(detail);
    return detail;
}

PaymentDetail *Payment::addInputSelector()
{
    // only one input selector allowed
    for (auto d : m_paymentDetails) {
        if (d->isInputs()) {
            // un-collapse, but not add
            d->setCollapsed(false);
            return nullptr;
        }
    }
    auto detail = new PaymentDetailInputs(this);
    addDetail(detail);
    return detail;
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
    doAutoPrepare();
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

    QTimer::singleShot(1000, this, [=]() {
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

void Payment::doAutoPrepare()
{
    if (!m_autoPrepare)
        return;
    try {
        prepare();
    } catch(...) {}
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
    doAutoPrepare();
}

bool Payment::txPrepared() const
{
    return m_txPrepared;
}

int Payment::paymentAmountFiat() const
{
    int amount = 0;
    PaymentDetailInputs *inputSelector = nullptr;
    for (auto i = m_paymentDetails.crbegin(); i != m_paymentDetails.crend(); ++i) {
        auto *detail = *i;
        if (detail->isInputs())
            inputSelector = detail->toInputs(); // find the one input selector if there is one
        if (!detail->isOutput()) // only care about outputs from here
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
                auto wallet = m_wallet; // use the one we prepare()d from if available
                if (wallet == nullptr)
                    wallet = m_account->wallet();
                totalBch = wallet->balanceConfirmed() + wallet->balanceUnconfirmed();
            }

            return (totalBch * m_fiatPrice / 10000000 + 5) / 10;
        }
        amount += out->paymentAmountFiat();
    }
    return amount;
}

double Payment::paymentAmount() const
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
            auto wallet = m_wallet; // use the one we prepare()d from if available
            if (wallet == nullptr)
                wallet = m_account->wallet();
            return wallet->balanceConfirmed() + wallet->balanceUnconfirmed();
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

