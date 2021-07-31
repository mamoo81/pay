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
#include "AccountInfo.h"
#include "WalletHistoryModel.h"
#include "FloweePay.h"

#include <p2p/PrivacySegment.h>

#include <base58.h>
#include <primitives/key.h>
#include <cashaddr.h>


WalletSecret::WalletSecret(int id, QObject *parent)
    : QObject(parent),
      m_id(id)
{
}

int WalletSecret::id() const
{
    return m_id;
}

bool WalletSecret::used() const
{
    return m_used;
}

void WalletSecret::setUsed(bool newUsed)
{
    if (m_used == newUsed)
        return;
    m_used = newUsed;
    emit usedChanged();
}

bool WalletSecret::usedSchnorr() const
{
    return m_usedSchnorr;
}

void WalletSecret::setUsedSchnorr(bool newUsedSchnorr)
{
    if (m_usedSchnorr == newUsedSchnorr)
        return;
    m_usedSchnorr = newUsedSchnorr;
    emit usedSchnorrChanged();
}

const QString &WalletSecret::address() const
{
    return m_address;
}

void WalletSecret::setAddress(const QString &newAddress)
{
    if (m_address == newAddress)
        return;
    m_address = newAddress;
    emit addressChanged();
}

qint64 WalletSecret::saldo() const
{
    return m_saldo;
}

void WalletSecret::setSaldo(qint64 newSaldo)
{
    if (m_saldo == newSaldo)
        return;
    m_saldo = newSaldo;
    emit saldoChanged();
}

void WalletSecret::populate(const Wallet::WalletSecret &secret, qint64 saldo)
{
    CashAddress::Content c;
    c.hash.resize(20);
    memcpy(c.hash.data(), secret.address.begin(), 20);
    c.type = CashAddress::PUBKEY_TYPE;
    auto prefix = FloweePay::instance()->chainPrefix();
    auto ad = CashAddress::encodeCashAddr(prefix, c);
    setAddress(QString::fromStdString(ad).mid(prefix.size() + 1));

    switch (secret.signatureType) {
    case Wallet::SignedAsEcdsa:
        setUsed(true);
        setUsedSchnorr(false);
        break;
    case Wallet::SignedAsSchnorr:
        setUsed(true);
        setUsedSchnorr(true);
        break;
    case Wallet::NotUsedYet:
        setUsed(false);
        setUsedSchnorr(true);
        break;
    }

    if (saldo >= 0)
        setSaldo(saldo);
}

QString WalletSecret::fetchPrivateKey() const
{
    AccountInfo *ai = qobject_cast<AccountInfo*>(parent());
    assert(ai);
    const auto secrets = ai->m_wallet->walletSecrets();
    auto iter = secrets.find(m_id);
    assert(iter != secrets.end());

    CBase58Data d;
    d.setData(iter->second.privKey, FloweePay::instance()->chain() == P2PNet::MainChain ? CBase58Data::Mainnet : CBase58Data::Testnet);
    return QString::fromStdString(d.ToString());
}


// --------------------------------------------------------------------------


AccountInfo::AccountInfo(Wallet *wallet, QObject *parent)
    : QObject(parent),
      m_wallet(wallet)
{
    connect(wallet, SIGNAL(utxosChanged()), this, SIGNAL(utxosChanged()), Qt::QueuedConnection);
    connect(wallet, SIGNAL(balanceChanged()), this, SIGNAL(balanceChanged()), Qt::QueuedConnection);
    connect(wallet, SIGNAL(lastBlockSynchedChanged()), this, SIGNAL(lastBlockSynchedChanged()), Qt::QueuedConnection);
    connect(wallet, SIGNAL(paymentRequestsChanged()), this, SIGNAL(paymentRequestsChanged()), Qt::QueuedConnection);
    connect(wallet, SIGNAL(walletSecretChanged(int)), this, SLOT(updateWalletSecret(int)), Qt::QueuedConnection);
}

int AccountInfo::id() const
{
    return m_wallet->segment()->segmentId();
}

double AccountInfo::balanceConfirmed() const
{
    return static_cast<double>(m_wallet->balanceConfirmed());
}

double AccountInfo::balanceUnconfirmed() const
{
    return static_cast<double>(m_wallet->balanceUnconfirmed());
}

double AccountInfo::balanceImmature() const
{
    return static_cast<double>(m_wallet->balanceImmature());
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

int AccountInfo::lastBlockSynched() const
{
    if (!m_wallet->segment())
        return 0;
    return m_wallet->segment()->lastBlockSynched();
}

WalletHistoryModel *AccountInfo::historyModel()
{
    if (m_model == nullptr)
        m_model.reset(new WalletHistoryModel(m_wallet, this));
    return m_model.get();
}

void AccountInfo::setDefaultWallet(bool isDefault)
{
    auto segment =m_wallet->segment();
    if (!segment)
        return;
    if (segment->priority() == isDefault ? PrivacySegment::First : PrivacySegment::Normal)
        return;
    segment->setPriority(isDefault ? PrivacySegment::First : PrivacySegment::Normal);
    emit isDefaultWalletChanged();
}

bool AccountInfo::isDefaultWallet()
{
    auto segment =m_wallet->segment();
    if (!segment)
        return false;
    return segment->priority() == PrivacySegment::First;
}

bool AccountInfo::userOwnedWallet()
{
    return m_wallet->userOwnedWallet();
}

QList<QObject *> AccountInfo::paymentRequests() const
{
    QList<QObject*> answer;
    for (auto *pr : m_wallet->paymentRequests()) {
        answer.append(pr);
    }
    return answer;
}

TransactionInfo* AccountInfo::txInfo(int walletIndex, QObject *parent)
{
    Q_ASSERT(parent);
    auto info = new TransactionInfo(parent);
    m_wallet->fetchTransactionInfo(info, walletIndex);
    return info;
}

QObject *AccountInfo::createPaymentRequest(QObject *parent)
{
    return new PaymentRequest(m_wallet, parent);
}

bool AccountInfo::isSingleAddressAccount() const
{
    return m_wallet->isSingleAddressWallet();
}

const QList<WalletSecret *> &AccountInfo::walletSecrets()
{
    if (m_walletSecrets.isEmpty()) {
        // lazy Initialize
        const auto secrets = m_wallet->walletSecrets();
        for (auto i = secrets.begin(); i != secrets.end(); ++i) {
            WalletSecret *s = new WalletSecret(i->first, this);
            s->populate(i->second, m_wallet->saldoForPrivateKey(i->first));
            m_walletSecrets.append(s);
        }
    }
    return m_walletSecrets;
}

void AccountInfo::updateWalletSecret(int id)
{
    if (m_walletSecrets.isEmpty())
        return;

    for (auto ws : m_walletSecrets) {
        if (ws->id() == id) {
            auto map = m_wallet->walletSecrets();
            auto iter = map.find(id);
            assert(iter != map.end());
            ws->populate(iter->second, -1);
        }
    }
}
