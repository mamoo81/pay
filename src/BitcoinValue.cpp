/*
 * This file is part of the Flowee project
 * Copyright (C) 2020-2022 Tom Zander <tom@flowee.org>
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
#include "BitcoinValue.h"
#include "FloweePay.h"
#include "PriceDataProvider.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QTimer>
#include <QMimeData>

BitcoinValue::BitcoinValue(QObject *parent)
    : QObject(parent),
      m_value(0)
{
    connect (FloweePay::instance(), &FloweePay::unitChanged, this, [this]() {
        if (!m_fiatMode) {
            if (m_valueSource == UserInput) { // m_typedNumber is leading
                // we follow what the user actually typed. For crypto values anyway.
                setStringValue(m_typedNumber);
            } else {
                QTimer::singleShot(10, this, SLOT(processNumberValue()));
            }
        }
    });
    connect (FloweePay::instance()->prices(), &PriceDataProvider::currencySymbolChanged, this, [this]() {
        if (m_fiatMode)
            setDisplayCents(FloweePay::instance()->prices()->displayCents());
    });
}

qint64 BitcoinValue::value() const
{
    return m_value;
}

void BitcoinValue::setValue(qint64 value, ValueSource source)
{
    if (m_value == value)
        return;
    m_value = value;
    m_valueSource = source;
    emit valueChanged();

    if (source == FromNumber)
        QTimer::singleShot(10, this, SLOT(processNumberValue()));
}

void BitcoinValue::moveLeft()
{
    if (m_cursorPos > 0)
        setCursorPos(m_cursorPos - 1);
}

void BitcoinValue::moveRight()
{
    if (m_typedNumber.size() > m_cursorPos)
        setCursorPos(m_cursorPos + 1);
}

bool BitcoinValue::insertNumber(QChar number)
{
    if (!number.isNumber())
        throw std::runtime_error("Only numbers can be inserted in insertNumber");
    auto backup(m_typedNumber);
    int pos = m_typedNumber.indexOf('.');
    const int unitConfigDecimals = m_fiatMode ? (m_displayCents ? 2 : 0)
                                              : FloweePay::instance()->unitAllowedDecimals();
    if (pos > -1 && m_cursorPos > pos && m_typedNumber.size() - pos - unitConfigDecimals > 0)
        return false;
    int cursorPosNow = m_cursorPos;
    m_typedNumber.insert(cursorPosNow, number);
    while (((pos < 0 && m_typedNumber.size() > 1) || pos > 1) && m_typedNumber.startsWith('0')) {
        --cursorPosNow;
        m_typedNumber = m_typedNumber.mid(1);
    }

    if (!setStringValue(m_typedNumber)) {
        // revert due to error
        m_typedNumber = backup;
        return false;
    }
    setCursorPos(cursorPosNow + 1);
    emit enteredStringChanged();
    return true;
}

bool BitcoinValue::addSeparator()
{
    if (m_typedNumber.indexOf('.') != -1 || (m_fiatMode && !m_displayCents))
        return false;
    m_typedNumber.insert(m_cursorPos, '.');
    int movedPlaces = 1;
    if (m_typedNumber.size() == 1) {
        ++movedPlaces;
        m_typedNumber = "0.";
    }
    setCursorPos(m_cursorPos + movedPlaces);
    setStringValue(m_typedNumber);
    emit enteredStringChanged();
    return true;
}

void BitcoinValue::paste()
{
    auto *mimeData = QGuiApplication::clipboard()->mimeData();
    QString newValue;
    if (mimeData->hasText())
        newValue = mimeData->text();
    else if (mimeData->hasHtml()) // apps like telegram use this, even if they don't have the html tags wrapping it.
        newValue = mimeData->html();

    setStringValue(newValue);
    m_valueSource = FromNumber;
    QTimer::singleShot(10, this, SLOT(processNumberValue()));
}

bool BitcoinValue::backspacePressed()
{
    int cursorPosition = m_cursorPos;
    if (m_typedNumber.isEmpty())
        return false;
    if (m_typedNumber.size() <= 1) {
        m_typedNumber.clear();
        cursorPosition = 0;
    } else {
        m_typedNumber.remove(--cursorPosition, 1);
    }
    setStringValue(m_typedNumber);
    emit enteredStringChanged();
    setCursorPos(cursorPosition);
    return true;
}

void BitcoinValue::reset()
{
    m_value = 0;
    m_typedNumber.clear();
    m_cursorPos = 0;
    emit enteredStringChanged();
    emit cursorPosChanged();
    emit valueChanged();
}

bool BitcoinValue::setStringValue(const QString &value)
{
    int separator = value.indexOf('.');
    QStringView before;
    QString after;
    if (separator == -1) {
        before = value;
        after = "000000000";
    } else {
        before = QStringView(value).left(separator);
        after = value.mid(separator + 1);
    }
    qint64 newVal = before.toLong();
    const int unitConfigDecimals = m_fiatMode ? 2 : FloweePay::instance()->unitAllowedDecimals();
    for (int i = 0; i < unitConfigDecimals; ++i) {
        newVal *= 10;
    }
    while (after.size() < unitConfigDecimals)
        after += '0';

    newVal += QStringView(after).left(unitConfigDecimals).toInt();

    if (!m_fiatMode) {
        constexpr int64_t Coin = 100000000; // num satoshis in a single coin
        constexpr int64_t MaxMoney = 21000000 * Coin; // number of maxim monies ever to be created
        if (newVal > MaxMoney)
            return false;
    }
    setValue(newVal, UserInput);
    return true;
}

bool BitcoinValue::displayCents() const
{
    return m_displayCents;
}

void BitcoinValue::setDisplayCents(bool newDisplayCents)
{
    if (m_displayCents == newDisplayCents)
        return;
    m_displayCents = newDisplayCents;
    emit displayCentsChanged();

    // a BitcoinValue instance does not change its 'fiatMode' in usage, just at construction time
    // as a result we can do the action on displayCents changes here.
    if (!m_fiatMode)
        return;

    if (!m_displayCents) {
            // special case this. Drop the separator if the new currency has none.
        int index = m_typedNumber.indexOf('.');
        if (index > 0) {
            m_typedNumber = m_typedNumber.left(index);
            setCursorPos(std::min(index, m_cursorPos));
        }
    }
    setStringValue(m_typedNumber);
}

bool BitcoinValue::fiatMode() const
{
    return m_fiatMode;
}

void BitcoinValue::setFiatMode(bool newFiatMode)
{
    if (m_fiatMode == newFiatMode)
        return;
    m_fiatMode = newFiatMode;
    emit fiatModeChanged();

    if (m_fiatMode)
        setDisplayCents(FloweePay::instance()->prices()->displayCents());
}

QString BitcoinValue::enteredString() const
{
    return m_typedNumber;
}

int BitcoinValue::cursorPos() const
{
    return m_cursorPos;
}

void BitcoinValue::setCursorPos(int cp)
{
    if (m_cursorPos == cp)
        return;
    m_cursorPos = cp;
    emit cursorPosChanged();
}

/*
 * Use m_value as the 'user input' and update
 * the cursor pos and the m_typedNumber variables in a heuristically pleasing manner.
 */
void BitcoinValue::processNumberValue()
{
    if (m_valueSource == UserInput)
        return;

    const int unitConfigDecimals = m_fiatMode ? (m_displayCents ? 2 : 0)
                                              : FloweePay::instance()->unitAllowedDecimals();
    m_typedNumber = QString::number(m_value / pow(10, m_fiatMode ? 2 : unitConfigDecimals), 'f', unitConfigDecimals);
    if (unitConfigDecimals > 0) { // there is a comma separator in the string
        int length;
        if (m_typedNumber.startsWith("0."))
            length = -1;
        else
            length = m_typedNumber.indexOf('.')  - 1;
        for (int i = length + 1; i < m_typedNumber.size(); ++i) {
            if (m_typedNumber.at(i).isDigit() && m_typedNumber.at(i).unicode() != '0') {
                length = i;
            }
        }
        m_typedNumber = m_typedNumber.left(length + 1);
    }
    setCursorPos(m_typedNumber.size());
    emit enteredStringChanged();
}
