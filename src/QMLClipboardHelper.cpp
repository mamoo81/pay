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
#include "FloweePay.h"
#include "QMLClipboardHelper.h"
#include "WalletEnums.h"

#include <QGuiApplication>
#include <QClipboard>
#include <QTimer>
#include <QMimeData>

QMLClipboardHelper::QMLClipboardHelper(QObject *parent)
    : QObject{parent},
    m_filter(Addresses)
{
    // when the app regains main-app status, check for changes in the clipboard.
    auto guiApp = qobject_cast<QGuiApplication*>(QCoreApplication::instance());
    assert(guiApp);
    connect(guiApp, &QGuiApplication::applicationStateChanged, this, [=](Qt::ApplicationState state) {
        if (state == Qt::ApplicationActive)
            delayedParse();
    });
    // parse it in the next event, when properties are fully set
    delayedParse();
}

void QMLClipboardHelper::parseClipboard()
{
    m_delayedParseStarted = false;
    if (!m_enabled)
        return;
    auto *mimeData = QGuiApplication::clipboard()->mimeData();
    QString text;
    if (mimeData->hasText())
        text = mimeData->text();
    else if (mimeData->hasHtml())
        text = mimeData->html();

    if (text.isEmpty() || m_filter == NoFilter) {
        setClipboardText(text);
        return;
    }
    // often found whitespace, make it all spaces.
    text = text.replace(QLatin1String("<br>"), QLatin1String(" "));
    text = text.replace(QLatin1String("\n"), QLatin1String(" "));
    text = text.replace(QLatin1String("\r"), QLatin1String(" "));
    text = text.trimmed();

    const QString prefix = QString::fromStdString(chainPrefix()) + ":";
    QString result;
    auto type = FloweePay::instance()->identifyString(text);
    if (type == WalletEnums::Unknown || type == WalletEnums::PartialMnemonicWithTypo || type == WalletEnums::PartialMnemonic) { // try harder
        auto index = text.indexOf(prefix);
        if (index >= 0) {
            auto end = text.indexOf(' ', index + prefix.size());
            result = text.mid(index, end);
        }
        else {
            // find the address if it doesn't have the prefix.
            for (auto &word : text.split(' ', Qt::SkipEmptyParts)) {
                if (word.length() > 40 && word.length() < 50) {
                    auto id = FloweePay::instance()->identifyString(prefix + word);
                    if (id == WalletEnums::CashPKH || id == WalletEnums::CashSH) {
                        result = prefix + word;
                        break;
                    }
                }
            }
        }
        type = FloweePay::instance()->identifyString(result);
        text = result;
    }

    bool itsAHit = false;
    if ((type == WalletEnums::Unknown || type == WalletEnums::PartialMnemonicWithTypo
              || type == WalletEnums::PartialMnemonic)
            && m_filter.testAnyFlag(AddressUrl)) { // try finding AddressUrl (aka bip21)
        const int endOfAddress = text.indexOf('?');
        if (endOfAddress > 0 && text.startsWith(prefix))
            itsAHit = true;
    }
    else if (m_filter.testAnyFlag(Addresses)
            && (type == WalletEnums::CashPKH || type == WalletEnums::CashSH))
        itsAHit = true;
    else if (m_filter.testAnyFlag(LegacyAddresses)
              && (type == WalletEnums::LegacyPKH || type == WalletEnums::LegacySH))
        itsAHit = true;

    if (itsAHit)
        setClipboardText(text);
}

void QMLClipboardHelper::setClipboardText(const QString &text)
{
    if (m_text == text)
        return;
    m_text = text;
    emit textChanged();
}

void QMLClipboardHelper::delayedParse()
{
    if (m_delayedParseStarted)
        return;
    QTimer::singleShot(0, this, &QMLClipboardHelper::parseClipboard);
    m_delayedParseStarted = true;
}

bool QMLClipboardHelper::enabled() const
{
    return m_enabled;
}

void QMLClipboardHelper::setEnabled(bool newEnabled)
{
    if (m_enabled == newEnabled)
        return;
    m_enabled = newEnabled;
    emit enabledChanged();
    parseClipboard();
}

QString QMLClipboardHelper::clipboardText() const
{
    return m_text;
}

void QMLClipboardHelper::setFilter(QMLClipboardHelper::Types filter)
{
    if (m_filter == filter)
        return;
    m_filter = filter;
    emit filterChanged();
    delayedParse();
}

QMLClipboardHelper::Types QMLClipboardHelper::filter() const
{
    return m_filter;
}
