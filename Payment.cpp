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
    for (auto detail : m_paymentDetails) {
        if (!detail->valid())
            return false;
    }
    return true;
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
    for (auto detail : m_paymentDetails) {
        if (detail->type() == PayToAddress) {
            assert(!seenMaxAmount); // only allowed once.
            auto o = detail->toOutput();
            seenMaxAmount |= o->maxAllowed() && o->maxSelected();
            totalOut += o->paymentAmount();
            builder.appendOutput(o->paymentAmount());

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
            assert(ok); // mismatch between PaymentDetailOutput::setAddress and this method...
        }
    }

    auto tx = builder.createTransaction();

    // find and add outputs that can be used to pay for our required output
    int64_t change = -1;
    const auto funding = m_wallet->findInputsFor(seenMaxAmount ? -1 : totalOut, m_fee, tx.size(), change);
    if (funding.outputs.empty()) { // not enough funds.
        m_error = tr("Not enough funds in account to make payment!");
        emit errorChanged();
        return;
    }
    if (!m_error.isEmpty()) {
        m_error.clear();
        emit errorChanged();
    }

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

void Payment::reset()
{
    m_fee = 1;
    m_txPrepared = false;
    m_preferSchnorr = true;
    m_error.clear();
    m_tx = Tx();
    m_assignedFee = 0;
    m_infoObject.reset();
    m_sentPeerCount = 0;
    m_rejectedPeerCount = 0;

    qDeleteAll(m_paymentDetails);
    m_paymentDetails.clear();
    emit paymentDetailsChanged(); // work around too-lazy QML not updating if the number of items is identical
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
}

void Payment::addDetail(PaymentDetail *detail)
{
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
        connect (detail, SIGNAL(paymentAmountChanged()), this, SLOT(paymentAmountChanged()));
        connect (detail, SIGNAL(fiatAmountChanged()), this, SLOT(paymentAmountChanged()));
    }
    emit paymentDetailsChanged();
    emit validChanged(); // pretty sure we are invalid after ;-)
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
        if (detail->isInputs()) {
            detail->toInputs()->setWallet(account->wallet());
            break;
        }
    }
}

int Payment::fiatPrice() const
{
    return m_fiatPrice;
}

void Payment::setFiatPrice(int newFiatPrice)
{
    if (m_fiatPrice == newFiatPrice)
        return;
    m_fiatPrice = newFiatPrice;
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
    if (m_sentPeerCount == 0)
        return TxNotSent;
    if (m_rejectedPeerCount > 0)
        return TxRejected;
    if (m_sentPeerCount > 1) {
        if (m_infoObject.get() == nullptr)
            return TxBroadcastSuccess;
        return TxWaiting;
    }
    return TxSent1;
}

void Payment::addExtraOutput()
{
    // only the last in the sequence can have 'max'
    for (auto d : m_paymentDetails) {
        if (d->isOutput()) {
            d->toOutput()->setMaxAllowed(false);
        }
    }
    addDetail(new PaymentDetailOutput(this));
}

void Payment::addInputSelector()
{
    // only one input selector allowed
    for (auto d : m_paymentDetails) {
        if (d->isInputs())
            return;
    }
    addDetail(new PaymentDetailInputs(this));
}

void Payment::remove(PaymentDetail *detail)
{
    const auto count = m_paymentDetails.removeAll(detail);
    if (count) {
        emit paymentDetailsChanged();

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
    assert(!m_paymentDetails.isEmpty());
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
    if (++m_sentPeerCount >= 2) {
        // if two peers requested the tx, then wait a bit and check on the status
        QTimer::singleShot(5 * 1000, [=]() {
            if (m_sentPeerCount - m_rejectedPeerCount > 2) {
                // When enough peers received the transaction stop broadcasting it.
                m_infoObject.reset();
            }
            emit broadcastStatusChanged();
        });
    }
    emit broadcastStatusChanged();
}

void Payment::txRejected(short reason, const QString &message)
{
    // reason is hinted using BroadcastTxData::RejectReason
    logCritical() << "Transaction rejected" << reason << message;
    ++m_rejectedPeerCount;
    emit broadcastStatusChanged();
}

void Payment::paymentAmountChanged()
{
    // Find the output that has 'max' and give it a change to recalculate the effective values.
    auto out = qobject_cast<PaymentDetailOutput*>(sender());
    if (out == nullptr)
        return;
    if (out->maxSelected() && out->maxAllowed())
        return;
    for (auto i = m_paymentDetails.rbegin(); i != m_paymentDetails.rend(); ++i) {
        auto *detail = *i;
        if (detail->isOutput()) {
            if (detail != sender())
                detail->toOutput()->recalcMax();
            break; // only the last detail can be a 'max'
        }
    }
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


// ///////////////////////////////////////////////

PaymentDetailInputs::PaymentDetailInputs(Payment *parent)
    : PaymentDetail(parent, Payment::InputSelector),
      m_wallet(parent->currentAccount()->wallet()),
      m_model(m_wallet)
{
    m_model.setSelectionGetter(std::bind(&PaymentDetailInputs::isRowIncluded, this, std::placeholders::_1));
}

void PaymentDetailInputs::setWallet(Wallet *wallet)
{
    if (wallet == m_wallet)
        return;
    m_wallet = wallet;
    m_model.setWallet(m_wallet);

    emit selectedCountChanged();
    emit selectedValueChanged();
}

WalletCoinsModel *PaymentDetailInputs::coins()
{
    return &m_model;
}

void PaymentDetailInputs::setRowIncluded(int row, bool on)
{
    assert(m_wallet);
    const auto id = m_model.outRefForRow(row);
    auto &selection = m_selectionModels[m_wallet];
    const auto iter = selection.rows.find(id);
    const bool wasThere = iter != selection.rows.end();
    if (on == wasThere) // no change
        return;

    const auto value = m_wallet->utxoOutputValue(Wallet::OutputRef(id));
    if (on) {
        assert(!wasThere);
        selection.rows.insert(id);
        selection.selectedValue += value;
        ++selection.selectedCount;
    }
    else {
        assert(wasThere);
        selection.rows.erase(iter);
        selection.selectedValue -= value;
        --selection.selectedCount;
    }
    emit selectedCountChanged();
    emit selectedValueChanged();

    m_model.updateRow(row);
}

bool PaymentDetailInputs::isRowIncluded(uint64_t rowId)
{
    assert(m_wallet);
    auto a = m_selectionModels.find(m_wallet);
    if (a == m_selectionModels.end())
        return false;
    const auto &selection = a->second;
    return selection.rows.find(rowId) != selection.rows.end();
}

void PaymentDetailInputs::setOutputLocked(int row, bool lock)
{
    assert(m_wallet);
    m_model.setOutputLocked(row, lock);
}

void PaymentDetailInputs::selectAll()
{
    assert(m_wallet);
    auto &selection = m_selectionModels[m_wallet];
    for (int i = m_model.rowCount() - 1; i >= 0; --i) {
        auto const id = m_model.outRefForRow(i);
        assert(id >= 0);
        if (selection.rows.find(id) == selection.rows.end()) {
            selection.rows.insert(id);
            m_model.updateRow(i);

            const auto value = m_wallet->utxoOutputValue(Wallet::OutputRef(id));
            selection.selectedValue += value;
            ++selection.selectedCount;
        }
    }
    emit selectedCountChanged();
    emit selectedValueChanged();
}

void PaymentDetailInputs::unselectAll()
{
    assert(m_wallet);
    auto a = m_selectionModels.find(m_wallet);
    if (a == m_selectionModels.end())
        return;
    auto selection = a->second;
    m_selectionModels.erase(a);
    for (auto rowId : selection.rows) {
        // tell the model to tell the view the data has changed.
        m_model.updateRow(rowId);
    }
    emit selectedCountChanged();
    emit selectedValueChanged();
}

double PaymentDetailInputs::selectedValue() const
{
    assert(m_wallet);
    auto a = m_selectionModels.find(m_wallet);
    if (a == m_selectionModels.end())
        return false;
    const auto &selection = a->second;
    return static_cast<double>(selection.selectedValue);
}

int PaymentDetailInputs::selectedCount() const
{
    auto a = m_selectionModels.find(m_wallet);
    if (a == m_selectionModels.end())
        return 0;
    const auto &selection = a->second;
    return selection.selectedCount;
}


// ///////////////////////////////////////////////

PaymentDetailOutput::PaymentDetailOutput(Payment *parent)
    : PaymentDetail(parent, Payment::PayToAddress)
{

}

double PaymentDetailOutput::paymentAmount() const
{
    if (!m_fiatFollows) {
        Payment *p = qobject_cast<Payment*>(parent());
        assert(p);
        if (p->fiatPrice() == 0)
            return 0;

        return static_cast<double>(m_fiatAmount * 100000000L / p->fiatPrice());
    }
    if (m_maxAllowed && m_maxSelected) {
        Payment *p = qobject_cast<Payment*>(parent());
        assert(p);
        assert(p->currentAccount());
        auto wallet = p->currentAccount()->wallet();
        assert(wallet);
        auto amount = wallet->balanceConfirmed() + wallet->balanceUnconfirmed();
        for (auto detail : p->m_paymentDetails) {
            if (detail == this) // max is only allowed in the last of the outputs
                break;
            if (detail->isOutput())
                amount -= detail->toOutput()->paymentAmount();
        }
        return static_cast<double>(amount);
    }
    return static_cast<double>(m_paymentAmount);
}

void PaymentDetailOutput::setPaymentAmount(double amount_)
{
    qint64 amount = static_cast<qint64>(amount_);
    if (m_paymentAmount == amount)
        return;
    m_paymentAmount = amount;
    // implicit changes first, it changes the representation
    setFiatFollows(true);
    setMaxSelected(false);
    emit fiatAmountChanged();
    emit paymentAmountChanged();
    checkValid();
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
    m_address = address;

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
    }

    emit addressChanged();
    checkValid();
}

void PaymentDetailOutput::checkValid()
{
    bool valid = !m_formattedTarget.isEmpty();
    valid = valid
            && ((m_maxSelected && m_maxAllowed)
                || (m_fiatFollows && m_paymentAmount > 600)
                || (!m_fiatFollows && m_fiatAmount > 0));
    setValid(valid);
}

bool PaymentDetailOutput::maxSelected() const
{
    return m_maxSelected;
}

void PaymentDetailOutput::setMaxSelected(bool on)
{
    if (m_maxSelected == on)
        return;
    m_maxSelected = on;
    // implicit change first, it changes the representation
    setFiatFollows(true);
    emit maxSelectedChanged();
    emit fiatAmountChanged();
    emit paymentAmountChanged();
    checkValid();
}

void PaymentDetailOutput::recalcMax()
{
    if (m_maxSelected && m_maxAllowed) {
        emit fiatAmountChanged();
        emit paymentAmountChanged();
    }
}

bool PaymentDetailOutput::fiatFollows() const
{
    return m_fiatFollows;
}

void PaymentDetailOutput::setFiatFollows(bool on)
{
    if (m_fiatFollows == on)
        return;
    m_fiatFollows = on;
    emit fiatFollowsChanged();
}

int PaymentDetailOutput::fiatAmount() const
{
    if (m_fiatFollows) {
        Payment *p = qobject_cast<Payment*>(parent());
        assert(p);
        if (p->fiatPrice() == 0)
            return 0;
        qint64 amount = m_paymentAmount;
        if (m_maxAllowed && m_maxSelected) {
            assert(p->currentAccount());
            auto wallet = p->currentAccount()->wallet();
            assert(wallet);
            amount = wallet->balanceConfirmed() + wallet->balanceUnconfirmed();
            for (auto detail : p->m_paymentDetails) {
                if (detail == this) // max is only allowed in the last of the outputs
                    break;
                if (detail->isOutput())
                    amount -= detail->toOutput()->paymentAmount();
            }
        }

        return (amount * p->fiatPrice() / 10000000 + 5) / 10;
    }
    return m_fiatAmount;
}

void PaymentDetailOutput::setFiatAmount(int amount)
{
    if (m_fiatAmount == amount)
        return;
    m_fiatAmount = amount;
    // implicit changes first, it changes the representation
    setFiatFollows(false);
    setMaxSelected(false);
    emit fiatAmountChanged();
    emit paymentAmountChanged();
    checkValid();
}

const QString &PaymentDetailOutput::formattedTarget() const
{
    return m_formattedTarget;
}

bool PaymentDetailOutput::maxAllowed() const
{
    return m_maxAllowed;
}

void PaymentDetailOutput::setMaxAllowed(bool max)
{
    if (m_maxAllowed == max)
        return;
    m_maxAllowed = max;
    emit maxAllowedChanged();

    if (max == false && m_paymentAmount == -1)
        setPaymentAmount(0);
}
