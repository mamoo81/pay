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
#include "AccountInfo.h"
#include "WalletHistoryModel.h"
#include "TransactionInfo.h"
#include "FloweePay.h"

#include <p2p/PrivacySegment.h>
#include <utils/base58.h>
#include <utils/primitives/key.h>
#include <utils/cashaddr.h>
#include <SyncSPVAction.h>

#include <QTimer>


AccountInfo::AccountInfo(Wallet *wallet, QObject *parent)
    : QObject(parent),
      m_wallet(wallet),
      m_config(m_wallet),
      m_initialBlockHeight(wallet->segment()->lastBlockSynched())
{
    connect(wallet, SIGNAL(utxosChanged()), this, SIGNAL(utxosChanged()), Qt::QueuedConnection);
    connect(wallet, SIGNAL(balanceChanged()), this, SLOT(balanceHasChanged()), Qt::QueuedConnection);
    connect(wallet, &Wallet::lastBlockSynchedChanged, m_wallet, [=]() {
        if (m_initialBlockHeight < 0)
            m_initialBlockHeight = m_wallet->segment()->lastBlockSynched();
        emit lastBlockSynchedChanged();
        emit timeBehindChanged();
    }, Qt::QueuedConnection);
    connect(wallet, SIGNAL(encryptionChanged()), this, SLOT(walletEncryptionChanged()), Qt::QueuedConnection);
    connect(wallet, SIGNAL(userOwnedChanged()), this, SIGNAL(userOwnedChanged()), Qt::QueuedConnection);
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
   return QDateTime::fromSecsSinceEpoch(timestamp);
}

QString AccountInfo::timeBehind() const
{
    const int accountHeight = lastBlockSynched();
    if (accountHeight <= 0) { // Special case.
        // May be accounts that only expect tx in the future.
        // may be wallets that are fully encrypted.
        if (m_wallet->encryption() == Wallet::FullyEncrypted && !m_wallet->isDecrypted())
            return tr("Offline");
        return tr("Wallet: Up to date");
    }
    const int chainHeight = FloweePay::instance()->chainHeight();

    const int diff = chainHeight - accountHeight;
    if (diff < 0) // we are ahead???
        return "--";
    auto days = diff / 144.;
    auto weeks = diff / 1008.;
    if (days > 10) {
        const int w = static_cast<int>(weeks);
        return tr("Behind: %1 weeks, %2 days", "counter on weeks", w).arg(w).arg(static_cast<int>(days) % 7);
    }
    auto hours = diff / 6.;
    if (hours > 48)
        return tr("Behind: %1 days", "", std::ceil(days)).arg(std::ceil(days));
    if (diff == 0)
        return tr("Up to date");
    if (diff < 3 && !isArchived())
        return tr("Updating");
    return tr("Still %1 hours behind", "", std::ceil(hours)).arg(std::ceil(hours));
}

WalletHistoryModel *AccountInfo::historyModel()
{
    if (m_model == nullptr) {
        m_model.reset(new WalletHistoryModel(m_wallet, this));
    }
    return m_model.get();
}

WalletSecretsModel *AccountInfo::secretsModel()
{
    if (m_secretsModel == nullptr) {
        m_secretsModel.reset(new WalletSecretsModel(m_wallet, this));
    }
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
    return QDateTime::fromSecsSinceEpoch(timestamp);
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
    if (archived)
        closeWallet();
}

void AccountInfo::balanceHasChanged()
{
    emit balanceChanged();

    /* us getting called is very likely due to a new transaction that has been made known to the wallet. */
    if (!m_hasFreshTransactions && m_wallet->isDecrypted()) {
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

void AccountInfo::walletEncryptionChanged()
{
    auto m = m_model.release();
    if (m)
        m->deleteLater();
    auto s = m_secretsModel.release();
    if (s)
        s->deleteLater();
    emit modelsChanged();
    emit encryptionChanged();
    emit nameChanged();
}

bool AccountInfo::isPrivate() const
{
    assert(m_config.isValid());
    return m_config.isPrivate();
}

void AccountInfo::setIsPrivate(bool newIsPrivate)
{
    assert(m_config.isValid());
    m_config.setIsPrivate(newIsPrivate);
    emit isPrivateChanged();
}

bool AccountInfo::allowInstaPay() const
{
    assert(m_config.isValid());
    return m_config.allowInstaPay();
}

void AccountInfo::setAllowInstaPay(bool newAllowInstaPay)
{
    assert(m_config.isValid());
    m_config.setAllowInstaPay(newAllowInstaPay);
    emit instaPayAllowedChanged();
}

int AccountInfo::fiatInstaPayLimit(const QString &currencyCode) const
{
    assert(m_config.isValid());
    return m_config.fiatInstaPayLimit(currencyCode);
}

void AccountInfo::setFiatInstaPayLimit(const QString &currencyCode, int cents)
{
    assert(m_config.isValid());
    m_config.setFiatInstaPayLimit(currencyCode, cents);
    emit instaPayLimitChanged(currencyCode);
}

bool AccountInfo::countBalance() const
{
    assert(m_config.isValid());
    return m_config.countBalance();
}

void AccountInfo::setCountBalance(bool newCountBalance)
{
    assert(m_config.isValid());
    m_config.setCountBalance(newCountBalance);
}

QStringList AccountInfo::instaPayLimitCurrencies() const
{
    assert(m_config.isValid());
    return m_config.fiatInstaPayLimits().keys();
}

int AccountInfo::initialBlockHeight() const
{
    return m_initialBlockHeight;
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

bool AccountInfo::needsPinToPay() const
{
    return m_wallet->encryption() != Wallet::NotEncrypted;
}

bool AccountInfo::needsPinToOpen() const
{
    return m_wallet->encryption() == Wallet::FullyEncrypted;
}

bool AccountInfo::isDecrypted() const
{
    return m_wallet->isDecrypted();
}

void AccountInfo::setPrimaryAccount(bool isPrimary)
{
    auto segment = m_wallet->segment();
    if (!segment)
        return;
    if ((segment->priority() == PrivacySegment::First) == isPrimary)
        return;
    segment->setPriority(isPrimary ? PrivacySegment::First : PrivacySegment::Normal);
    // emit also notifies the portfolio manager that makes sure
    // other accounts get their primary-ness unset. There can be only one!
    emit isPrimaryAccountChanged();
}

bool AccountInfo::isPrimaryAccount() const
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

TransactionInfo* AccountInfo::txInfo(int walletIndex, QObject *parent)
{
    Q_ASSERT(parent);
    auto info = new TransactionInfo(parent);
    m_wallet->fetchTransactionInfo(info, walletIndex);
    return info;
}

void AccountInfo::encryptPinToPay(const QString &password)
{
    m_wallet->setEncryption(Wallet::SecretsEncrypted, password);
}

void AccountInfo::encryptPinToOpen(const QString &password)
{
    if (m_closeWalletTimer)
        m_closeWalletTimer->stop();
    m_wallet->setEncryption(Wallet::FullyEncrypted, password);
}

bool AccountInfo::decrypt(const QString &password)
{
    if (isDecrypted()) {
        logCritical() << "Requested to decrypt an already decrypted wallet";
        return false;
    }
    const bool shouldAutoclose = m_wallet->encryption() == Wallet::SecretsEncrypted;
    const bool success = m_wallet->decrypt(password);
    if (shouldAutoclose && success) {
        if (!m_closeWalletTimer) {
            m_closeWalletTimer = new QTimer(this);
            connect (m_closeWalletTimer, &QTimer::timeout, m_wallet, [=]() {
                m_wallet->forgetEncryptedSecrets();
            });
        }
        m_closeWalletTimer->stop();
        m_closeWalletTimer->start(200 * 1000); // 200s
    }
    return success;
}

void AccountInfo::closeWallet()
{
    // this forgets secrets
    m_wallet->forgetEncryptedSecrets();
    if (m_closeWalletTimer)
        m_closeWalletTimer->stop();
    setHasFreshTransactions(false);
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

bool AccountInfo::isElectrumMnemonic() const
{
    return m_wallet->isElectrumMnemonic();
}

QString AccountInfo::hdDerivationPath() const
{
    return m_wallet->derivationPath();
}

QString AccountInfo::xpub() const
{
    return m_wallet->xpub();
}
