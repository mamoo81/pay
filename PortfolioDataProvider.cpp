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
#include "PortfolioDataProvider.h"
#include "PortfolioDataProvider_p.h"
#include "FloweePay.h"

#include <QFile>
#include <QSettings>
#include <QTimer>
#include <base58.h>
#include <cashaddr.h>

#define DEFAULT_WALLET "default_wallet_type"

AccountInfo::AccountInfo(Wallet *wallet, QObject *parent)
    : QObject(parent),
      m_wallet(wallet)
{
    connect(wallet, SIGNAL(utxosChanged()), this, SIGNAL(utxosChanged()), Qt::QueuedConnection);
}

int AccountInfo::id() const
{
    return m_wallet->segment()->segmentId();
}

double AccountInfo::balance() const
{
    return static_cast<double>(m_wallet->balance());
}

int AccountInfo::unspentOutputCount() const
{
    return m_wallet->unspentOutputCount();
}

int AccountInfo::historicalOutputCount() const
{
    return m_wallet->historicalOutputCount();
}

void AccountInfo::setName(const QString &name)
{
    if (m_wallet->name() == name)
        return;
    m_wallet->setName(name);
    emit nameChanged();
}

QString AccountInfo::name() const
{
    return m_wallet->name();
}

WalletHistoryModel *AccountInfo::historyModel()
{
    if (m_model == nullptr)
        m_model.reset(new WalletHistoryModel(m_wallet, this));
    return m_model.get();
}


// //////////////////////////////////////////////////////////////////////

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
    case FloweePay::CashPKH:
    case FloweePay::LegacyPKH: {
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
    if (m_address.isEmpty() || m_paymentAmount < 600)
        throw std::runtime_error("Can not create transaction, missing data");
    TransactionBuilder builder;
    builder.appendOutput(m_paymentAmount);
    bool ok = false;
    std::string s = m_address.toStdString();
    CBase58Data legacy;
    if (legacy.SetString(s)) {
        if (legacy.isMainnetPkh()) {
            assert(legacy.data().size() == 20);
            builder.pushOutputPay2Address(CKeyID(reinterpret_cast<const char*>(legacy.data().data())));
            ok = true;
        }
        // TODO p2sh
    }
    else {
        CashAddress::Content c = CashAddress::decodeCashAddrContent(s, "bitcoincash");
        if (!c.hash.empty()) {
            if (c.type == CashAddress::PUBKEY_TYPE) {
                builder.pushOutputPay2Address(CKeyID(reinterpret_cast<const char*>(c.hash.data())));
                ok = true;
            }
            // else if (c.type == CashAddress::SCRIPT_TYPE)
            // TODO p2sh
        }
    }
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
        builder.selectOutput(changeOutput);
        builder.setOutputValue(change + diff);
        m_assignedFee += diff;
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


// //////////////////////////////////////////////////////////////////////

PortfolioDataProvider::PortfolioDataProvider(QObject *parent) : QObject(parent)
{
    connect (FloweePay::instance(), &FloweePay::walletsChanged, [=]() {
        for (auto w : FloweePay::instance()->wallets()) {
            if (!m_accounts.contains(w)) {
                addWalletAccount(w);
            }
        }
    });
}

QList<AccountInfo *> PortfolioDataProvider::accounts() const
{
    return m_accountInfos;
}

AccountInfo *PortfolioDataProvider::current() const
{
    if (m_currentAccount >= 0 && m_currentAccount < m_accountInfos.size())
        return m_accountInfos.at(m_currentAccount);
    return nullptr;
}

void PortfolioDataProvider::setCurrent(AccountInfo *item)
{
    QSettings appConfig;
    if (item) {
        int index = m_accountInfos.indexOf(item);
        if (index == m_currentAccount)
            return;
        m_currentAccount = index;

        appConfig.setValue(DEFAULT_WALLET, m_accounts.at(index)->segment()->segmentId());
    }
    else {
        appConfig.remove(DEFAULT_WALLET);
        m_currentAccount = -1;
    }
    emit currentChanged();
}

QObject *PortfolioDataProvider::startPayToAddress(const QString &address, BitcoinValue *bitcoinValue)
{
    assert(bitcoinValue);
    if (m_currentAccount == -1 || bitcoinValue == nullptr)
        return nullptr;
    auto p = new Payment(m_accounts.at(m_currentAccount), bitcoinValue->value());
    try {
        p->setTargetAddress(address);
        return p;
    } catch (...) {
        delete p;
        throw;
    }
}

void PortfolioDataProvider::selectDefaultWallet()
{
    QSettings appConfig;
    const int defaultWalletSegment = appConfig.value(DEFAULT_WALLET, -1).toInt();
    if (defaultWalletSegment == -1)
        return;
    for (int i = 0; i < m_accounts.size(); ++i) {
        auto wallet = m_accounts.at(i);
        assert(wallet->segment());
        if (wallet->segment()->segmentId() == defaultWalletSegment) {
            m_currentAccount = i;
            emit currentChanged();
            break;
        }
    }
}

void PortfolioDataProvider::addWalletAccount(Wallet *wallet)
{
    if (m_accounts.contains(wallet))
        return;
    m_accounts.append(wallet);
    m_accountInfos.append(new AccountInfo(wallet, this));
    accountsChanged();
}
