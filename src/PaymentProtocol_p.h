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
#ifndef PAYMENTPROTOCOL_P_H
#define PAYMENTPROTOCOL_P_H

#include "PaymentProtocol.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>

#include <streaming/BufferPool.h>

class Payment;

class PaymentProtocolBip21 : public PaymentProtocol
{
    Q_OBJECT
public:
    explicit PaymentProtocolBip21(Payment *payment);

    void setUri(const QString &uri) override;
};

class PaymentProtocolBip70 : public PaymentProtocol
{
    Q_OBJECT
public:
    explicit PaymentProtocolBip70(Payment *payment);

    void setUri(const QString &uri) override;

private slots:
    void fetchedRequest();
    void sentTransaction();
    void errored(QNetworkReply::NetworkError err);
    void sslErrors(const QList<QSslError> &errors);

    void sendReply();

private:
    QNetworkAccessManager m_network;
    QNetworkReply *m_reply = nullptr;
    Streaming::BufferPool m_pool;

    QDateTime m_requestCreated;
    QDateTime m_requestExpiry;

    QString m_memo;
    QString m_replyUrl;
    Streaming::ConstBuffer m_merchantData;

    struct Output {
        uint64_t amount = 0;
        Streaming::ConstBuffer script;
    };
    std::vector<Output> m_outputs;
};

#endif
