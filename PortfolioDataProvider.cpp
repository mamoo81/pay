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
#include "FloweePay.h"
#include "FloweePay.h"

#include <QFile>
#include <base58.h>
#include <cashaddr.h>

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

qint64 AccountInfo::balance() const
{
    return m_wallet->balance();
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

Payment::Payment(Wallet *wallet)
    : m_wallet(wallet),
      m_fee(1)
{
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

void Payment::setPaymentAmount(qint64 amount)
{
    if (m_paymentAmount == amount)
        return;
    m_paymentAmount = amount;
    emit amountChanged();
}

qint64 Payment::paymentAmount()
{
    return m_paymentAmount;
}

void Payment::setTargetAddress(const QString &address)
{
    if (m_address == address)
        return;
    switch (FloweePay::instance()->identifyString(address)) {
    case FloweePay::CashPKH:
    case FloweePay::CashSH:
    case FloweePay::LegacyPKH:
    case FloweePay::LegacySH:
        m_address = address;
        emit targetAddressChanged();
        break;
    default:
        throw std::runtime_error("Address not recognized");
    }
}

QString Payment::targetAddress()
{
    return m_address;
}

void Payment::approveAndSend()
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
    for (auto ref : prevOuts) {
        builder.appendInput(m_wallet->txid(ref), ref.outputIndex());
        auto output = m_wallet->txOutout(ref);
        builder.pushInputSignature(m_wallet->unlockKey(ref), output.outputScript, output.outputValue);
    }

    if (change > 1000) {
        // notice that the findInputsFor() will try really really hard to avoid us
        // having change greater than 100 and less than 1000.
        // But if we hit that, its better to give it to the miners than to
        // create a tiny change UTXO
        builder.appendOutput(change);
        builder.pushOutputPay2Address(m_wallet->newChangeAddress());
    }

    tx = builder.createTransaction();

    /*
     * TODO
     *  - call to wallet to mark outputs locked and save tx.
     *  - actually broadcast tx.
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
    if (item) {
        int index = m_accountInfos.indexOf(item);
        if (index == m_currentAccount)
            return;
        m_currentAccount = index;
    }
    else {
        m_currentAccount = -1;
    }
    emit currentChanged();
}

QObject *PortfolioDataProvider::startPayToAddress(const QString &address, qint64 amount)
{
    if (m_currentAccount == -1)
        return nullptr;
    auto p = new Payment(m_accounts.at(m_currentAccount));
    try {
        p->setPaymentAmount(amount);
        p->setTargetAddress(address);
        return p;
    } catch (...) {
        delete p;
        throw;
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
