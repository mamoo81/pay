/*
 * This file is part of the Flowee project
 * Copyright (C) 2020 Tom Zander <tom@flowee.org>
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
#ifndef PAYMENTINFOOBJECT_P_H
#define PAYMENTINFOOBJECT_P_H

#include <QObject>
#include <BroadcastTxData.h>

class Tx;
class Payment;

class PaymentInfoObject : public QObject, public BroadcastTxData
{
    Q_OBJECT
public:
    PaymentInfoObject(Payment *payment, const Tx &tx);

    void txRejected(RejectReason reason, const std::string &message) override;
    void sentOne() override;
    uint16_t privSegment() const override;

signals:
    void sentOneFired();
    void txRejectedFired(short reason, const QString &message);

private:
    Payment *m_parent;
};

#endif
