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

#include "PaymentDetailInputs_p.h"
#include "Wallet.h"
#include "Wallet_p.h" // for BYTES_PER_OUTPUT
#include "AccountInfo.h"

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
    setValid(selection.selectedValue > 547);
    emit validChanged(); // the Payment needs to do the math as well, if inputs and outputs match
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
    if (lock)
        setRowIncluded(row, false);
}

void PaymentDetailInputs::selectAll()
{
    assert(m_wallet);
    auto &selection = m_selectionModels[m_wallet];
    for (int i = m_model.rowCount() - 1; i >= 0; --i) {
        auto const id = m_model.outRefForRow(i);
        if (selection.rows.find(id) == selection.rows.end()) {
            const Wallet::OutputRef ref(id);
            if (m_wallet->isLocked(ref))
                continue;

            selection.rows.insert(id);
            m_model.updateRow(i);

            const auto value = m_wallet->utxoOutputValue(ref);
            selection.selectedValue += value;
            ++selection.selectedCount;
        }
    }
    emit selectedCountChanged();
    emit selectedValueChanged();
    setValid(true);
    emit validChanged(); // the Payment needs to do the math as well, if inputs and outputs match
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
    setValid(false);
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

Wallet::OutputSet PaymentDetailInputs::selectedInputs(int64_t output, int feePerByte, int txSize, int64_t &change) const
{
    Wallet::OutputSet answer;
    assert(m_wallet);
    auto a = m_selectionModels.find(m_wallet);
    if (a == m_selectionModels.end())
        return answer;
    const auto &selection = a->second;
    int fee = feePerByte * (txSize + BYTES_PER_OUTPUT * selection.rows.size());
    change = 0;
    if (output != -1) { // an output of -1 means that there was an ouput that takes 'the rest', leaving no change.
        change = selection.selectedValue - output - fee;
        if (change < 0) // return an empty answer if the input can't pay the outputs plus expected fees.
            return answer;
    }

    answer.totalSats = selection.selectedValue;
    answer.outputs.reserve(selection.rows.size());
    for (auto ref : selection.rows) {
        answer.outputs.push_back(Wallet::OutputRef(ref));
    }
    return answer;
}
