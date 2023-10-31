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

PaymentProtocol::PaymentProtocol(Payment *payment, QObject *parent)
    : QObject{parent},
    m_payment(payment)
{
    assert(payment);
}

PaymentProtocol *PaymentProtocol::create(Payment *target, const QString &uri, QObject *parent)
{
    assert(target->paymentDetails().size() == 1);
    PaymentProtocol *pp = nullptr;
    if (uri.length() < 20 || !uri.startsWith("bitcoincash:")) {
        logWarning(10003) << "Payment protocol unrecognized URI:" << uri;
        return nullptr;
    }
    else if (uri.at(12) == '?' && uri.at(13) == 'r') {
        pp = new PaymentProtocolBip70(target, parent);
    }
    else if (uri.at(12) == 'q' || uri.at(12) == 'p') {
        pp = new PaymentProtocolBip21(target, parent);
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

PaymentProtocolBip21::PaymentProtocolBip21(Payment *payment, QObject *parent)
    : PaymentProtocol(payment, parent)
{
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

PaymentProtocolBip70::PaymentProtocolBip70(Payment *payment, QObject *parent)
    : PaymentProtocol(payment, parent)
{
}

void PaymentProtocolBip70::setUri(const QString &uri)
{
    /*
     * Make sure that we account for the async nature of bip70 and also the fact
     * that the bip70 usage implies that the user should not edit the amount.
     * So, we set a dummy amount in the payment in order to make the UI aware
     * of the fact that the amount is pre-filled.
     */
    m_payment->setPaymentAmount(1);

    if (FloweePay::instance()->isOffline()) {
        logCritical(10003) << "App is offline, can't handle BIP70 payment";
        m_payment->addWarning(Payment::OfflineWarning);
        finished();
        return;
    }

    // Mark the detail as not editable as soon as possible.
    assert(m_payment->paymentDetails().size() == 1);
    auto *first = m_payment->paymentDetails().at(0);
    assert(first);
    auto *out = qobject_cast<PaymentDetailOutput*>(first);
    assert(out);
    out->setEditable(false);

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
    m_payment->setPaymentAmount(0); // reset to zero

    m_pool.reserve(m_reply->size());
    logInfo(10003) << "BIP 70 request fetched, payload:" << m_reply->size();
    auto readBytes = m_reply->read(m_pool.data(), m_pool.capacity());
    if (readBytes == -1) {
        m_payment->addWarning(Payment::DownloadFailed);
        return;
    }
    auto buf = m_pool.commit(readBytes);

    const auto ssl = m_reply->sslConfiguration();
    if (ssl.isNull() || ssl.sessionProtocol() < QSsl::TlsV1_3) {
        m_payment->setAllowInstaPay(false); // require user approval
        m_payment->addWarning(Payment::InsecureTransport);
    }

    const auto contentType = m_reply->header(QNetworkRequest::ContentTypeHeader);
    delete m_reply;
    m_reply = nullptr;

    // protocol buffers PaymentRequest with embedded PaymentDetails / Outputs
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
            logCritical(10003) << buf.toString();
            auto mime(contentType.toString());
            int semicolon = mime.indexOf(';');
            if (semicolon > 0)
                mime = mime.left(semicolon);
            if ((mime == "text/html" || mime == "text/plain") && buf.size() < 100) {
                auto str = buf.toString();
                if (str.find('<') == std::string::npos) {
                    m_payment->forwardError(QString::fromLatin1(str));
                    return;
                }
            }
            m_payment->forwardError(tr("Payment request unreadable"));
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
                    // lets not translate this, its unlikely to ever happen.
                    m_payment->forwardError("not for mainchain");
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
            case 6: // reply URL
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
    if (m_requestExpiry < QDateTime::currentDateTimeUtc()) {
        logWarning(10003) << "   + expired";
        m_payment->forwardError(tr("Payment request expired"));
    }

    m_payment->setUserComment(m_memo);
    assert(m_payment->paymentDetails().size() == 1);
    auto *firstOut = dynamic_cast<PaymentDetailOutput*>(m_payment->paymentDetails().first());
    assert(firstOut);
    // lets add all the outputs
    for (const auto &out : m_outputs) {
        auto simple = CashAddress::extractAddressFromScript(out.script);
        PaymentDetailOutput *addressDetail = firstOut;
        firstOut = nullptr;
        if (!addressDetail) // first was already used, create next
            addressDetail = m_payment->addExtraOutput()->toOutput();
        if (simple.hash.empty()) {
            logInfo(10003) << "Output is not an address. Using bytearray instead";
            addressDetail->setOutputScript(out.script);
        } else {
            auto address = CashAddress::encodeCashAddr(chainPrefix(), simple);
            logDebug(10003) << "Payment to" << address;
            addressDetail->setAddress(QString::fromLatin1(address));
        }
        addressDetail->setPaymentAmount(out.amount);
        addressDetail->setEditable(false);
    }

    // then we have to wait for the user to Ok it and only after broadcast started,
    // we send the transaction also to the payment processor.
    connect (m_payment, &Payment::approvedByUser, this, [=]() {
        assert(m_payment->txPrepared());
        if (m_requestExpiry < QDateTime::currentDateTimeUtc()) {
            m_payment->forwardError(tr("Payment request expired"));
            return;
        }
        sendReply();
   });

    // Help the payment object start the broadcast if its properties automate this.
    if (m_payment->autoPrepare()) {
        try { m_payment->prepare(); } catch (...) {}
        m_payment->setAllowInstaPay(false); // avoid instapay if the prepare failed.
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
    m_payment->confirmReceivedOk();
    deleteLater();
}

void PaymentProtocolBip70::errored(QNetworkReply::NetworkError err)
{
    logCritical(10003) << m_reply->errorString();
    if (m_reply->error() == 302) {
        logFatal(10003) << " to:" << m_reply->header(QNetworkRequest::LocationHeader).toString();
    }
    else if (m_reply->error() == QNetworkReply::SslHandshakeFailedError) {
        m_payment->setAllowInstaPay(false);
        m_payment->addWarning(Payment::InsecureTransport);
    }
    else if (m_reply->error() <= 99) {
        // covers all the remote network errors.
        // notify the user why nothing is happening.
        m_payment->addWarning(Payment::DownloadFailed);
    }
}

void PaymentProtocolBip70::sslErrors(const QList<QSslError> &errors)
{
    m_payment->setAllowInstaPay(false);
    m_payment->addWarning(Payment::InsecureTransport);
    logCritical(10003) << "multiple errors:" << errors.size();
    for (const auto &e : errors) {
        logCritical(10003) << " " << e.errorString();
    }
}

void PaymentProtocolBip70::sendReply()
{
    if (m_sent)
        return;

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
