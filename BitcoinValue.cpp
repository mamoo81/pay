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

#include <QClipboard>
#include <QGuiApplication>

BitcoinValue::BitcoinValue(QObject *parent)
    : QObject(parent),
      m_value(0)
{
    connect (FloweePay::instance(), &FloweePay::unitChanged, this, [this]() {
        // we follow what the user actually typed.
        setStringValue(m_typedNumber);
    });
}

qint64 BitcoinValue::value() const
{
    return m_value;
}

void BitcoinValue::setValue(qint64 value)
{
    if (m_value == value)
        return;
    m_value = value;
    emit valueChanged();
    if (value && m_typedNumber.isEmpty()) {
        const int unitConfigDecimals = m_maxFractionalDigits == -1 ? FloweePay::instance()->unitAllowedDecimals() : m_maxFractionalDigits;
        m_typedNumber = QString::number(m_value / pow(10, unitConfigDecimals), 'f', unitConfigDecimals);
        if (unitConfigDecimals > 0) { // there is a comma separator in the string
            // remove trailing zeros
            while (m_typedNumber.endsWith('0') || m_typedNumber.endsWith('.'))
                m_typedNumber = m_typedNumber.left(m_typedNumber.size() - 1);
        }
        m_cursorPos = m_typedNumber.size();
    }
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

void BitcoinValue::insertNumber(QChar number)
{
    int pos = m_typedNumber.indexOf('.');
    const int unitConfigDecimals = m_maxFractionalDigits == -1 ? FloweePay::instance()->unitAllowedDecimals() : m_maxFractionalDigits;
    if (pos > -1 && m_cursorPos > pos && m_typedNumber.size() - pos - unitConfigDecimals > 0)
        return;
    int cursorPos = m_cursorPos;
    m_typedNumber.insert(cursorPos, number);
    while (((pos < 0 && m_typedNumber.size() > 1) || pos > 1) && m_typedNumber.startsWith('0')) {
        --cursorPos;
        m_typedNumber = m_typedNumber.mid(1);
    }

    setStringValue(m_typedNumber);
    setCursorPos(cursorPos + 1);
    emit enteredStringChanged();
}

void BitcoinValue::addSeparator()
{
    if (m_typedNumber.indexOf('.') == -1) {
        m_typedNumber.insert(m_cursorPos, '.');
        int movedPlaces = 1;
        if (m_typedNumber.size() == 1) {
            ++movedPlaces;
            m_typedNumber = "0.";
        }
        setCursorPos(m_cursorPos + movedPlaces);
        setStringValue(m_typedNumber);
        emit enteredStringChanged();
    }
}

void BitcoinValue::paste()
{
    QClipboard *clipboard = QGuiApplication::clipboard();
    assert(clipboard);
    setEnteredString(clipboard->text().trimmed());
}

void BitcoinValue::backspacePressed()
{
    int cursorPos = m_cursorPos;
    if (m_typedNumber.size() <= 1) {
        m_typedNumber.clear();
        cursorPos = 0;
    } else {
        m_typedNumber.remove(--cursorPos, 1);
    }
    setStringValue(m_typedNumber);
    emit enteredStringChanged();
    setCursorPos(cursorPos);
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

void BitcoinValue::setEnteredString(const QString &value)
{
    bool started = false;
    setCursorPos(0);
    m_typedNumber.clear();
    for (int i = 0; i < value.size(); ++i) {
        auto k = value.at(i);
        if (k.isDigit()) {
            started = true;
            insertNumber(k);
        }
        else if ((started || (value.size() > i + 1 && value.at(i+1).isDigit()))
                  && (k.unicode() == ',' || k.unicode() == '.')) {
            addSeparator();
        }
        else if (started)
            return;
    }
    if (!m_cursorPos)
        setValue(0);
}

void BitcoinValue::setStringValue(const QString &value)
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
    const int unitConfigDecimals = m_maxFractionalDigits == -1 ? FloweePay::instance()->unitAllowedDecimals() : m_maxFractionalDigits;
    for (int i = 0; i < unitConfigDecimals; ++i) {
        newVal *= 10;
    }
    while (after.size() < unitConfigDecimals)
        after += '0';

    newVal += QStringView(after).left(unitConfigDecimals).toInt();
    setValue(newVal);
}

QString BitcoinValue::enteredString() const
{
    const char decimalPoint = QLocale::system().decimalPoint().at(0).unicode();
    if (decimalPoint != '.') { // make the user-visible one always use the locale-aware decimalpoint.
        QString answer(m_typedNumber);
        answer.replace('.', decimalPoint);
        return answer;
    }
    return m_typedNumber;
}

int BitcoinValue::maxFractionalDigits() const
{
    return m_maxFractionalDigits;
}

void BitcoinValue::setMaxFractionalDigits(int newMaxFractionalDigits)
{
    if (m_maxFractionalDigits == newMaxFractionalDigits)
        return;
    m_maxFractionalDigits = newMaxFractionalDigits;
    emit maxFractionalDigitsChanged();

    // the behavior here assumes that this is a property most won't set and those that do
    // only set it once
    if (m_value) {
        m_typedNumber = QString::number(m_value / pow(10, m_maxFractionalDigits), 'f', m_maxFractionalDigits);
        m_cursorPos = m_typedNumber.size();
    }
}

void BitcoinValue::resetMaxFractionalDigits()
{
    setMaxFractionalDigits(-1);
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
