/*
 * This file is part of the Flowee project
 * Copyright (C) 2020-2021 Tom Zander <tom@flowee.org>
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
#ifndef BITCOINVALUE_H
#define BITCOINVALUE_H

#include <QObject>
#include <QString>

class BitcoinValue : public QObject
{
    Q_OBJECT
    Q_PROPERTY(double value READ realValue WRITE setRealValue NOTIFY valueChanged)
    Q_PROPERTY(QString enteredString READ enteredString WRITE setEnteredString NOTIFY enteredStringChanged)
    Q_PROPERTY(int maxFractionalDigits READ maxFractionalDigits WRITE setMaxFractionalDigits RESET resetMaxFractionalDigits NOTIFY maxFractionalDigitsChanged)
    Q_PROPERTY(int cursorPos READ cursorPos WRITE setCursorPos NOTIFY cursorPosChanged)
public:
    explicit BitcoinValue(QObject *parent = nullptr);

    qint64 value() const;
    void setValue(qint64 value);

    inline double realValue() {
        return m_value;
    }
    inline void setRealValue(double val) {
        setValue(val);
    }

    Q_INVOKABLE void moveLeft();
    Q_INVOKABLE void moveRight();
    Q_INVOKABLE void insertNumber(QChar number);
    Q_INVOKABLE void addSeparator();
    Q_INVOKABLE void paste();
    Q_INVOKABLE void backspacePressed();
    Q_INVOKABLE void reset();

    QString enteredString() const;
    void setEnteredString(const QString &s);

    /*
     * For fiat prices we want to limit the number of digits after
     * the unit separator. This allows us to do so.
     * Notice that reset unsets this making the number of digits follow the
     * Bitcoin Cash unit selected application-wide.
     */
    int maxFractionalDigits() const;
    void setMaxFractionalDigits(int newMaxFractionalDigits);
    void resetMaxFractionalDigits();

    int cursorPos() const;
    void setCursorPos(int newCursorPos);

signals:
    void valueChanged();
    void enteredStringChanged();
    void maxFractionalDigitsChanged();
    void cursorPosChanged();

private:
    // sets integer form of m_value from the \a value
    void setStringValue(const QString &value);

    qint64 m_value;
    QString m_typedNumber;
    int m_cursorPos = 0;
    int m_maxFractionalDigits = -1;
};

#endif
