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

#include "PaymentDetailOutput_p.h"
#include "PaymentDetailInputs_p.h"
#include "FloweePay.h"
#include "AccountInfo.h"

#include <base58.h>
#include <cashaddr.h>

#include <QUrl>
#include <QUrlQuery>

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
        /*
         * If the value is 'max' then we think per definition in satoshis.
         * We will count up all the outputs and subtract that from the base amount.
         *
         * The base amount is either the total spendable money in the wallet, or in
         * case there is an InputsSelector, the total amount selected there.
         */

        Payment *p = qobject_cast<Payment*>(parent());
        assert(p);
        assert(p->currentAccount());
        auto wallet = p->currentAccount()->wallet();
        assert(wallet);
        int64_t baseBalance = wallet->balanceConfirmed() + wallet->balanceUnconfirmed();
        int64_t outputs = 0;
        for (auto detail : p->m_paymentDetails) {
            if (detail == this) // max is only allowed in the last of the outputs
                continue;
            if (detail->isOutput())
                outputs += detail->toOutput()->paymentAmount();
            if (detail->isInputs())
                baseBalance = detail->toInputs()->selectedValue();
        }
        return static_cast<double>(std::max(0l, baseBalance - outputs));
    }
    return static_cast<double>(m_paymentAmount);
}

void PaymentDetailOutput::setPaymentAmount(double amount_)
{
    qint64 amount = static_cast<qint64>(amount_);
    if (m_paymentAmount == amount)
        return;
    if (m_maxAllowed && m_maxSelected) {
        // Check if we got set the exact value we reported.
        auto cur = paymentAmount();
        if (cur == amount)
            return;
    }
    m_paymentAmount = amount;
    // implicit changes first, it changes the representation
    setFiatFollows(true);
    setMaxSelected(false);
    emit paymentAmountChanged();
    checkValid();
}

const QString &PaymentDetailOutput::address() const
{
    return m_address;
}

void PaymentDetailOutput::setAddress(const QString &address_)
{
    const QString addressOrURL = address_.trimmed();
    if (m_address == addressOrURL)
        return;
    m_address = addressOrURL;
    const std::string &chainPrefixCopy = chainPrefix();

    /*
     * Users may paste an address that is really a payment url.
     * This basically means we may have a price added after a questionmark.
     * bitcoincash:qrejlchcwl232t304v8ve8lky65y3s945u7j2msl45?amount=2.1
     */
    int urlStart = addressOrURL.indexOf('?');
    if (urlStart > 0) {
        QUrl url(addressOrURL);
        auto query = QUrlQuery(url.query(QUrl::FullyDecoded));
        for (const auto &item : query.queryItems()) {
            if (item.first == "amount") {
                bool ok;
                auto amount = item.second.toDouble(&ok);
                if (ok)
                    setPaymentAmount(amount * 1E8);
            }
            else if (item.first == "label" || item.first == "message") {
                // message goes on the main payment..
                Payment *p = qobject_cast<Payment*>(parent());
                assert(p);
                p->setUserComment(item.second);
            }
        }

        m_address = addressOrURL.left(urlStart);
    }

    std::string encodedAddress;
    switch (FloweePay::instance()->identifyString(m_address)) {
    case WalletEnums::LegacyPKH: {
        CBase58Data legacy;
        auto ok = legacy.SetString(m_address.toStdString());
        assert(ok);
        assert(legacy.isMainnetPkh() || legacy.isTestnetPkh());
        CashAddress::Content c;
        c.hash = legacy.data();
        c.type = CashAddress::PUBKEY_TYPE;
        encodedAddress = CashAddress::encodeCashAddr(chainPrefixCopy, c);
        break;
    }
    case WalletEnums::CashPKH: {
        auto c = CashAddress::decodeCashAddrContent(m_address.toStdString(), chainPrefixCopy);
        assert (!c.hash.empty() && c.type == CashAddress::PUBKEY_TYPE);
        encodedAddress = CashAddress::encodeCashAddr(chainPrefixCopy, c);
        break;
    }
    case WalletEnums::LegacySH: {
        CBase58Data legacy;
        auto ok = legacy.SetString(m_address.toStdString());
        assert(ok);
        assert(legacy.isMainnetSh() || legacy.isTestnetSh());
        CashAddress::Content c;
        c.hash = legacy.data();
        c.type = CashAddress::SCRIPT_TYPE;
        encodedAddress = CashAddress::encodeCashAddr(chainPrefixCopy, c);
        break;
    }
    case WalletEnums::CashSH: {
        auto c = CashAddress::decodeCashAddrContent(m_address.toStdString(), chainPrefixCopy);
        assert (!c.hash.empty() && c.type == CashAddress::SCRIPT_TYPE);
        encodedAddress = CashAddress::encodeCashAddr(chainPrefixCopy, c);
        break;
    }
    default:
        break;
    }

    m_formattedTarget.clear();
    m_addressOk = !encodedAddress.empty();
    if (m_addressOk) {
        const auto size = chainPrefixCopy.size();
        // lets see if the encoded address is substantially different from the user-given address
        auto full = QString::fromStdString(encodedAddress);
        auto formattedTarget = full.mid(size + 1);
        if (full != m_address && formattedTarget != m_address)
            m_formattedTarget = full;
    }

    emit addressChanged();
    checkValid();
}

void PaymentDetailOutput::checkValid()
{
    bool valid = m_addressOk;
    valid = valid
            && ((m_maxSelected && m_maxAllowed)
                || (m_fiatFollows && m_paymentAmount > 600)
                || (!m_fiatFollows && m_fiatAmount > 0));
    setValid(valid);
}

bool PaymentDetailOutput::forceLegacyOk() const
{
    return m_forceLegacyOk;
}

void PaymentDetailOutput::setForceLegacyOk(bool newForceLegacyOk)
{
    if (m_forceLegacyOk == newForceLegacyOk)
        return;
    m_forceLegacyOk = newForceLegacyOk;
    emit forceLegacyOkChanged();
}

void PaymentDetailOutput::setWallet(Wallet *)
{
    if (m_maxAllowed && m_maxSelected) {
        emit paymentAmountChanged();
    }
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
    setFiatFollows(on);
    emit maxSelectedChanged();
    emit paymentAmountChanged();
    checkValid();
}

void PaymentDetailOutput::recalcMax()
{
    if (m_maxSelected && m_maxAllowed) {
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

int PaymentDetailOutput::paymentAmountFiat() const
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
            int64_t baseBalance = wallet->balanceConfirmed() + wallet->balanceUnconfirmed();
            int64_t outputs = 0;
            for (auto detail : p->m_paymentDetails) {
                if (detail == this) // max is only allowed in the last of the outputs
                    continue;
                if (detail->isOutput())
                    outputs += detail->toOutput()->paymentAmount();
                if (detail->isInputs())
                    baseBalance = detail->toInputs()->selectedValue();
            }
            amount = std::max(0l, baseBalance - outputs);
        }

        return (amount * p->fiatPrice() / 10000000 + 5) / 10;
    }
    return m_fiatAmount;
}

void PaymentDetailOutput::setPaymentAmountFiat(int amount)
{
    if (m_fiatAmount == amount)
        return;
    m_fiatAmount = amount;
    // implicit changes first, it changes the representation
    setFiatFollows(false);
    setMaxSelected(false);
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
    else
        emit paymentAmountChanged();
}
