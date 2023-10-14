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

QMLClipboardHelper::QMLClipboardHelper(QObject *parent)
    : QObject{parent}
{
    connect(QGuiApplication::clipboard(), &QClipboard::changed, this,
            [=](QClipboard::Mode changed) {
        if (changed == QClipboard::Clipboard) {
            parseClipboard();
        }
    });
    // parse it in the next event, so filters can be set by the user first.
    delayedParse();
}

void QMLClipboardHelper::parseClipboard()
{
    m_delayedParseStarted = false;
    QString result;
    const QString prefix = QString::fromStdString(chainPrefix()) + ":";
    auto text = QGuiApplication::clipboard()->text();
    if (m_filter == NoFilter) {
        setClipboardText(text);
        return;
    }

    auto type = FloweePay::instance()->identifyString(text);
    if (type == WalletEnums::Unknown) { // try harder
        auto index = text.indexOf(prefix);
        if (index >= 0) {
            auto end = text.indexOf(' ', index + 10);
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
    if (type == WalletEnums::Unknown && m_filter.testAnyFlag(AddressUrl)) { // try finding AddressUrl (aka bip21)
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
    else if (m_filter.testAnyFlag(Mnemonic) && type == WalletEnums::CorrectMnemonic)
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
