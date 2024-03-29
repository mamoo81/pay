/*
 * This file is part of the Flowee project
 * Copyright (C) 2023 Tom Zander <tom@flowee.org>
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
#ifndef PAYMENTPROTOCOL_H
#define PAYMENTPROTOCOL_H

#include <QObject>

class Payment;

class PaymentProtocol : public QObject
{
    Q_OBJECT
public:
    explicit PaymentProtocol(Payment *payment, QObject *parent);

    static PaymentProtocol* create(Payment *target, const QString &uri, QObject *parent);

    /**
     * A payment protocol decides if the target is a simple address or not.
     * If it is, then the UI will default to show the address we are paying to
     * as that helps end-users to verify they are paying to the right person.
     * More complex protocols probably want to turn this off as the address
     * idea is more hidden for them.
     */
    virtual bool simpleAddressTarget() const = 0;

protected:
    virtual void setUri(const QString &uri) = 0;
    void finished();

    Payment * const m_payment;
};

#endif
