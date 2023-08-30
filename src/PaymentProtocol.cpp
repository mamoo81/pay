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
        logWarning() << "Payment protocol unrecognized URI:" << uri;
        return nullptr;
    }
    else if (uri.at(12) == '?' && uri.at(13) == 'r') {
        pp = new PaymentProtocolBip70(target);
    }
    else if (uri.at(12) == 'q' || uri.at(12) == 'p') {
        pp = new PaymentProtocolBip21(target);
    }
    else {
        logWarning() << "Payment protocol missing support for uri:" << uri;
        return nullptr;
    }
    assert(pp);
    pp->setUri(uri);
    pp->finished();
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
        logCritical() << "App is offline, can't handle BIP70 payment";
        finished();
        return;
    }

    assert(uri.startsWith("bitcoincash:?r="));
    // bitcoincash:?r=https://
    QUrl url(uri.mid(15));
    m_reply = m_network.get(QNetworkRequest(url));
    logInfo() << "fetching bip70 file from:" << url.toString();
    connect(m_reply, SIGNAL(finished()), this, SLOT(fetchedRequest()));
}

void PaymentProtocolBip70::fetchedRequest()
{
    auto data = m_reply->readAll();
    QFile out("bip70Data");
    out.open(QIODevice::WriteOnly);
    out.write(data);

    delete m_reply;
    m_reply = nullptr;
}
