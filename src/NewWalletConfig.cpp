/*
 * This file is part of the Flowee project
 * Copyright (C) 2021 Tom Zander <tom@flowee.org>
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
#include "NewWalletConfig.h"
#include "Wallet.h"

NewWalletConfig::NewWalletConfig(Wallet *wallet)
    : m_wallet(wallet)
{
    // the entire intention is that QML can call some setters, due to its
    // declarative nature deleting outselves in the next event loop is quite safe
    // unless the QML dev does something really weird or the Qt-implementation
    // starts to do magic things
    deleteLater();
}

NewWalletConfig::~NewWalletConfig()
{
    if (m_wallet)
        emit m_wallet->startDelayedSave();
    m_wallet = nullptr;
}

QString NewWalletConfig::name() const
{
    assert(m_wallet);
    return m_wallet->name();
}

bool NewWalletConfig::forceSingleAddress() const
{
    assert(m_wallet);
    return m_wallet->isSingleAddressWallet();
}

void NewWalletConfig::setForceSingleAddress(bool on)
{
    assert(m_wallet);
    if (m_wallet->isHDWallet()) // a HD wallet with a single address makes no sense.
        return;
    if (m_wallet->isSingleAddressWallet() != on) {
        m_wallet->setSingleAddressWallet(on);
        emit SIChanged();
    }
}

int NewWalletConfig::accountId() const
{
    assert(m_wallet);
    assert(m_wallet->segment());
    return m_wallet->segment()->segmentId();
}
