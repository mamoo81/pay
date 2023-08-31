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
#include "PaymentProtocol.h"
#include "PaymentProtocol_p.h"
#include "Payment.h"
#include "PaymentDetailOutput_p.h"

#include <utils/streaming/ProtoParser.h>
#include <utils/streaming/ProtoBuilder.h>
#include <utils/cashaddr.h>

#include <FloweePay.h>
#include <QFile>
#include <QUrlQuery>

PaymentProtocol::PaymentProtocol(Payment *payment)
    : QObject{payment},
    m_payment(payment)
{
}

PaymentProtocol *PaymentProtocol::create(Payment *target, const QString &uri)
{
    PaymentProtocol *pp = nullptr;
    if (uri.length() < 20 || !uri.startsWith("bitcoincash:")) {
        logWarning(10003) << "Payment protocol unrecognized URI:" << uri;
        return nullptr;
    }
    else if (uri.at(12) == '?' && uri.at(13) == 'r') {
        pp = new PaymentProtocolBip70(target);
    }
    else if (uri.at(12) == 'q' || uri.at(12) == 'p') {
        pp = new PaymentProtocolBip21(target);
    }
    else {
        logWarning(10003) << "Payment protocol missing support for uri:" << uri;
        return nullptr;
    }
    assert(pp);
    pp->setUri(uri);
    return pp;
}

void PaymentProtocol::finished()
{
    if (m_payment->autoPrepare()) {
        try { m_payment->prepare(); } catch (...) {}
         /*
         * InstaPay is typically enabled together with auto-prepare and that gives
         * great UX for, well, instantly paying.
         *
         * The above will have created AND send the transaction.
         * BUT, if the prepare() failed, we should stop trying to do the 'instaPay'.
         * It failed, now let the user decide when to send.
         *
         * Either way, we can set it to false now.
         */
        m_payment->setAllowInstaPay(false);
    }
    deleteLater();
}

// ------------------------------------

PaymentProtocolBip21::PaymentProtocolBip21(Payment *payment)
    : PaymentProtocol(payment)
{
    assert(payment);
}

void PaymentProtocolBip21::setUri(const QString &uri)
{
    const QString addressOrURL = uri.trimmed();
    assert (m_payment->paymentDetails().size() == 1); // only one output
    auto out = qobject_cast<PaymentDetailOutput*>(m_payment->paymentDetails().at(0));
    assert(out);

    int urlStart = addressOrURL.indexOf('?');
    if (urlStart > 0) {
        QUrl url(addressOrURL);
        auto query = QUrlQuery(url.query(QUrl::FullyDecoded));
        for (const auto &item : query.queryItems()) {
            if (item.first == "amount") {
                bool ok;
                auto amount = item.second.toDouble(&ok);
                if (ok)
                    out->setPaymentAmount(amount * 1E8);
            }
            else if (item.first == "label" || item.first == "message") {
                // message goes on the main payment..
                m_payment->setUserComment(item.second);
            }
        }

        out->setAddress(addressOrURL.left(urlStart));
    }
    else {
        out->setAddress(addressOrURL);
    }
    finished();
}

PaymentProtocolBip70::PaymentProtocolBip70(Payment *payment)
    : PaymentProtocol(payment)
{
}

void PaymentProtocolBip70::setUri(const QString &uri)
{
    if (FloweePay::instance()->isOffline()) {
        logCritical(10003) << "App is offline, can't handle BIP70 payment";
        finished();
        return;
    }

    // uri typically starts with: bitcoincash:?r=https://
    assert(uri.startsWith("bitcoincash:?r="));
    QNetworkRequest req(uri.mid(15));
    req.setRawHeader("accept", "application/bitcoincash-paymentrequest"); // binary protocol
    req.setHeader(QNetworkRequest::UserAgentHeader, "flowee-pay");
    // accept: application/payment-request (JPP)
    m_reply = m_network.get(req);
    logInfo(10003) << "fetching bip70 file from:" << req.url().toString();
    connect(m_reply, SIGNAL(finished()), this, SLOT(fetchedRequest()));
    connect(m_reply, SIGNAL(errorOccurred(QNetworkReply::NetworkError)),
            this, SLOT(errored(QNetworkReply::NetworkError)));
    connect(m_reply, SIGNAL(sslErrors(QList<QSslError>)),
            this, SLOT(sslErrors(QList<QSslError>)));
}

void PaymentProtocolBip70::fetchedRequest()
{
    m_pool.reserve(m_reply->size());
    logInfo(10003) << "BIP 70 request fetched, payload:" << m_reply->size();
    auto readBytes = m_reply->read(m_pool.data(), m_pool.capacity());
    auto buf = m_pool.commit(readBytes);
    delete m_reply;
    m_reply = nullptr;

    // PaymentRequest with embedded PaymentDetails / Outputs
    Streaming::ProtoParser parser(buf);
    bool inDetails = false;
    bool inOutput = false;
    Output currentOutput;
    while (true) {
        const auto parsed = parser.next();
        if (parsed == Streaming::EndOfDocument) {
            if (inOutput)
                inOutput = false;
            else if (inDetails)
                inDetails = false;
            else
                break;
            assert (parser.depth() > 0);
            parser.closeData();
            continue;
        }
        else if (parsed == Streaming::Error) {
            logCritical(10003) << "failed parsing the protobuf";
            // TODO check if its a different format, or an error message?
            logCritical(10003) << buf.toString();
            return;
        }

        if (inOutput) {
            switch (parser.tag()) {
            case 1:
                currentOutput.amount = parser.longData();
                break;
            case 2:
                currentOutput.script = parser.bytesDataBuffer();
                m_outputs.push_back(currentOutput);
                break;
            }
        }
        else if (inDetails) {
            switch (parser.tag()) {
            case 1: // chain
                if (parser.stringData() != "main") {
                    logCritical(10003) << "Payment request for not mainchain. Ignoring";
                    return;
                }
                break;
            case 2: // outputs
                inOutput = true;
                parser.enterData();
                currentOutput = Output();
                break;
            case 3: // time
                m_requestCreated = QDateTime::fromSecsSinceEpoch(parser.longData());
                break;
            case 4: // expires
                m_requestExpiry = QDateTime::fromSecsSinceEpoch(parser.longData());
                break;
            case 5: // memo
                m_memo = QString::fromUtf8(parser.stringData());
                break;
            case 6: // memo
                m_replyUrl = QString::fromUtf8(parser.stringData());
                break;
            case 7: // merchant data
                m_merchantData = parser.bytesDataBuffer();
                break;
            }
        }
        else {
            switch (parser.tag()) {
            case 4:
                inDetails = true;
                parser.enterData();
                break;
            }
        }
    }

    logDebug(10003) << "Received a request for" << m_outputs.size() << "outputs";
    uint64_t amount = 0;
    for (const auto &o : m_outputs)
        amount += o.amount;
    logDebug(10003) << " total bch:" << amount;
    logDebug(10003) << "Reply to" << m_replyUrl;
    logDebug(10003) << "Created:" << m_requestCreated.toString();
    logDebug(10003) << "Expires:" << m_requestExpiry.toString();
    if (m_requestExpiry < QDateTime::currentDateTimeUtc())
        logWarning(10003) << "   + expired";

    for (const auto &out : m_outputs) {
        auto simple = CashAddress::extractAddressFromScript(out.script);
        if (simple.hash.empty()) {
            // TODO invent new payment output type
            logFatal(10003) << "Not an address";
            assert(false);
        } else {
            auto address = CashAddress::encodeCashAddr(chainPrefix(), simple);
            logDebug(10003) << "Payment to" << address;
            if (m_outputs.size() == 1) {
                // we place the data directly on the payment object because that
                // is the most compatible for consumers of its properties.
                m_payment->setTargetAddress(QString::fromLatin1(address));
                m_payment->setPaymentAmount(out.amount);
            }
            else {
                auto *addressDetail = m_payment->addExtraOutput()->toOutput();
                addressDetail->setAddress(QString::fromLatin1(address));
                addressDetail->setFiatFollows(true);
                addressDetail->setPaymentAmount(out.amount);
            }
        }
    }

    // TODO instead of this, we should be triggerd on the user askign the broadcast the transaction.
    if (m_payment->autoPrepare()) {
        try { m_payment->prepare(); } catch (...) {}
        m_payment->setAllowInstaPay(false);
        sendReply();
    }
    else {
        // then we have to wait for the user to Ok it and only after it has been prepared
        // do we send it.
        connect (m_payment, &Payment::txPreparedChanged, this, [=]() {
            if (m_payment->txPrepared())
                sendReply();
       });
    }
}

void PaymentProtocolBip70::sentTransaction()
{
    m_pool.reserve(m_reply->size());
    logInfo(10003) << "ACK received. Payload size:" << m_reply->size();
    auto readBytes = m_reply->read(m_pool.data(), m_pool.capacity());
    auto data = m_pool.commit(readBytes);
    delete m_reply;
    m_reply = nullptr;

    Streaming::ProtoParser::debug(data);
    logFatal(10003) << data.toString();
}

void PaymentProtocolBip70::errored(QNetworkReply::NetworkError err)
{
    logFatal(10003) << "Error: " << err;
    logFatal(10003) << m_reply->errorString();
    if (m_reply->error() == 302) {
        logFatal(10003) << " to:" << m_reply->header(QNetworkRequest::LocationHeader).toString();
    }
}

void PaymentProtocolBip70::sslErrors(const QList<QSslError> &errors)
{
    logFatal(10003) << "multiple errors:" << errors.size();
    for (const auto &e : errors) {
        logFatal(10003) << " " << e.errorString();
    }
}

void PaymentProtocolBip70::sendReply()
{
    // TODO check if we have not passed the expired time
    assert(m_payment->txPrepared());
    assert(!m_replyUrl.isEmpty());
    auto tx = m_payment->tx();

    m_pool.reserve(tx.size() + m_merchantData.size() + 40);
    Streaming::ProtoBuilder builder(m_pool);
    if (!m_merchantData.isEmpty())
        builder.add(1, m_merchantData);
    builder.add(2, tx.data());
    auto data = builder.buffer();
    logDebug() << " reply data size:" << data.size();

    // unfortunately, Qt uses its own data. So another copy is needed.
    QByteArray messageData(data.begin(), data.size());
    assert(messageData.size() == data.size());
    QNetworkRequest req(m_replyUrl);
    req.setRawHeader("accept", "application/bitcoincash-paymentack");
    req.setHeader(QNetworkRequest::UserAgentHeader, "flowee-pay");
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/bitcoincash-payment");
    m_reply = m_network.post(req, messageData);
    logInfo(10003) << "Sending bip70 reply to:" << req.url().toString();
    connect(m_reply, SIGNAL(finished()), this, SLOT(sentTransaction()));
    connect(m_reply, SIGNAL(errorOccurred(QNetworkReply::NetworkError)),
            this, SLOT(errored(QNetworkReply::NetworkError)));
    connect(m_reply, SIGNAL(sslErrors(QList<QSslError>)),
            this, SLOT(sslErrors(QList<QSslError>)));
}
