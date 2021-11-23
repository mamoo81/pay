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
#ifndef PAYMENT_DETAIL_INPUTS_H
#define PAYMENT_DETAIL_INPUTS_H

#include "Payment.h"

class PaymentDetailInputs : public PaymentDetail
{
    Q_OBJECT
    Q_PROPERTY(double selectedValue READ selectedValue NOTIFY selectedValueChanged)
    Q_PROPERTY(int selectedCount READ selectedCount NOTIFY selectedCountChanged)
    Q_PROPERTY(QAbstractListModel* coins READ coins CONSTANT)
public:
    explicit PaymentDetailInputs(Payment *parent);

    void setWallet(Wallet *wallet);

    /// returns the model that shows the unspent coins.
    WalletCoinsModel *coins();

    Q_INVOKABLE void setRowIncluded(int row, bool on);
    Q_INVOKABLE void setOutputLocked(int row, bool lock);
    Q_INVOKABLE void selectAll();
    Q_INVOKABLE void unselectAll();

    double selectedValue() const;
    int selectedCount() const;

signals:
    void selectedValueChanged();
    void selectedCountChanged();

private:
    bool isRowIncluded(uint64_t rowId);

    Wallet *m_wallet = nullptr;
    WalletCoinsModel m_model;

    struct Selection {
        quint64 selectedValue = 0;
        int selectedCount = 0;
        std::set<uint64_t> rows;
    };
    std::map<Wallet*, Selection> m_selectionModels;
};

inline PaymentDetailInputs *PaymentDetail::toInputs() {
    assert(m_type == Payment::InputSelector);
    return static_cast<PaymentDetailInputs*>(this);
}

#endif
