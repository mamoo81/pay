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
#include "AccountInfo.h"
#include "WalletHistoryModel.h"
#include "FloweePay.h"

#include <p2p/PrivacySegment.h>
#include <utils/base58.h>
#include <utils/primitives/key.h>
#include <utils/cashaddr.h>
#include <SyncSPVAction.h>


AccountInfo::AccountInfo(Wallet *wallet, QObject *parent)
    : QObject(parent),
      m_wallet(wallet)
{
    connect(wallet, SIGNAL(utxosChanged()), this, SIGNAL(utxosChanged()), Qt::QueuedConnection);
    connect(wallet, SIGNAL(balanceChanged()), this, SLOT(balanceHasChanged()), Qt::QueuedConnection);
    connect(wallet, SIGNAL(lastBlockSynchedChanged()), this, SIGNAL(lastBlockSynchedChanged()), Qt::QueuedConnection);
    connect(wallet, SIGNAL(lastBlockSynchedChanged()), this, SIGNAL(timeBehindChanged()), Qt::QueuedConnection);
    connect(wallet, SIGNAL(paymentRequestsChanged()), this, SIGNAL(paymentRequestsChanged()), Qt::QueuedConnection);
    connect(FloweePay::instance(), SIGNAL(headerChainHeightChanged()), this, SIGNAL(timeBehindChanged()));
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

QDateTime AccountInfo::lastBlockSynchedTime() const
{
    if (!m_wallet->segment() || m_wallet->segment()->lastBlockSynched() < 1)
        return QDateTime();
    auto timestamp = FloweePay::instance()->p2pNet()->blockchain().block(m_wallet->segment()->lastBlockSynched()).nTime;
   if (timestamp == 0)
       return QDateTime();
   return QDateTime::fromTime_t(timestamp);
}

QString AccountInfo::timeBehind() const
{
    const int accountHeight = lastBlockSynched();
    if (accountHeight <= 0) // For accounts that only expect tx in the future.
        return tr("Up to date");
    const int chainHeight = FloweePay::instance()->chainHeight();

    const int diff = chainHeight - accountHeight;
    if (diff < 0) // we are ahead???
        return "--";
    auto days = diff / 144.;
    auto weeks = diff / 1008.;
    if (days > 10)
        return tr("%1 weeks behind", "", std::ceil(weeks)).arg(std::ceil(weeks));
    auto hours = diff / 6.;
    if (hours > 48)
        return tr("%1 days behind", "", std::ceil(days)).arg(std::ceil(days));
    if (diff == 0)
        return tr("Up to date");
    if (diff < 3 && !isArchived())
        return tr("Updating");
    return tr("%1 hours behind", "", std::ceil(hours)).arg(std::ceil(hours));
}

WalletHistoryModel *AccountInfo::historyModel()
{
    if (m_model == nullptr)
        m_model.reset(new WalletHistoryModel(m_wallet, this));
    return m_model.get();
}

WalletSecretsModel *AccountInfo::secretsModel()
{
    if (m_secretsModel == nullptr)
        m_secretsModel.reset(new WalletSecretsModel(m_wallet, this));
    return m_secretsModel.get();
}

QDateTime AccountInfo::lastMinedTransaction() const
{
    assert(m_wallet);
    const int blockHeight = m_wallet->lastTransactionTimestamp();
    if (blockHeight <= 0)
        return QDateTime();
    auto timestamp = FloweePay::instance()->p2pNet()->blockchain().block(blockHeight).nTime;
    if (timestamp == 0)
        return QDateTime();
    return QDateTime::fromTime_t(timestamp);
}

bool AccountInfo::isArchived() const
{
    auto segment =m_wallet->segment();
    if (!segment)
        return true;
    return segment->priority() == PrivacySegment::OnlyManual;
}

void AccountInfo::setIsArchived(bool archived)
{
    auto segment = m_wallet->segment();
    if (!segment)
        return;
    if ((segment->priority() == PrivacySegment::OnlyManual) == archived)
        return;
    segment->setPriority(archived ? PrivacySegment::OnlyManual : PrivacySegment::Normal);
    emit isArchivedChanged();

    if (!archived && !FloweePay::instance()->isOffline()) {
         // make sure that we get peers for the no longer archived wallet.
        FloweePay::instance()->p2pNet()->addAction<SyncSPVAction>();
    }
}

void AccountInfo::balanceHasChanged()
{
    emit balanceChanged();

    /* us getting called is very likely due to a new transaction that has been made known to the wallet. */
    if (!m_hasFreshTransactions) {
        // check this
        const int blockHeight = m_wallet->lastTransactionTimestamp();
        if (m_lastTxHeight < blockHeight) {
            setHasFreshTransactions(true);
            m_lastTxHeight = blockHeight;
        }
        if (blockHeight == 0) // an unconfirmed one
            setHasFreshTransactions(true);
    }
}

bool AccountInfo::hasFreshTransactions() const
{
    return m_hasFreshTransactions;
}

void AccountInfo::setHasFreshTransactions(bool fresh)
{
    if (m_hasFreshTransactions == fresh)
        return;
    m_hasFreshTransactions = fresh;
    emit hasFreshTransactionsChanged();
}

void AccountInfo::setDefaultWallet(bool isDefault)
{
    auto segment = m_wallet->segment();
    if (!segment)
        return;
    if ((segment->priority() == PrivacySegment::First) == isDefault)
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

bool AccountInfo::isHDWallet() const
{
    return m_wallet->isHDWallet();
}

QString AccountInfo::hdWalletMnemonic() const
{
    return m_wallet->hdWalletMnemonic();
}

QString AccountInfo::hdDerivationPath() const
{
    return m_wallet->derivationPath();
}
