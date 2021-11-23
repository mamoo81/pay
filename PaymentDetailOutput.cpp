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

#include "PaymentDetailOutput.h"
#include "FloweePay.h"
#include "AccountInfo.h"

#include <base58.h>
#include <cashaddr.h>

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
