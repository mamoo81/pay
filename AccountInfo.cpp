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
#include "Wallet.h"
#include "WalletHistoryModel.h"
#include "FloweePay.h"

#include <p2p/PrivacySegment.h>

AccountInfo::AccountInfo(Wallet *wallet, QObject *parent)
    : QObject(parent),
      m_wallet(wallet)
{
    connect(wallet, SIGNAL(utxosChanged()), this, SIGNAL(utxosChanged()), Qt::QueuedConnection);
    connect(wallet, SIGNAL(balanceChanged()), this, SIGNAL(balanceChanged()), Qt::QueuedConnection);
    connect(wallet, SIGNAL(lastBlockSynchedChanged()), this, SIGNAL(lastBlockSynchedChanged()), Qt::QueuedConnection);
    connect(wallet, SIGNAL(paymentRequestsChanged()), this, SIGNAL(paymentRequestsChanged()), Qt::QueuedConnection);
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
