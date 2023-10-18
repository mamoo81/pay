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
    Q_PROPERTY(QString enteredString READ enteredString NOTIFY enteredStringChanged)
    Q_PROPERTY(int cursorPos READ cursorPos WRITE setCursorPos NOTIFY cursorPosChanged)

    /**
     * In 'fiat' mode we always use 2 digits behind the separator and we no longer follow
     * the application-wide setting for BCH-units.
     * Notice that for **display** the fiatDigits property is still relevant.
     */
    Q_PROPERTY(bool fiatMode READ fiatMode WRITE setFiatMode NOTIFY fiatModeChanged FINAL)
    /**
     * When fiatMode is true, this property is used to make the display and editing use
     * either 2 or zero digits behind the separator.
     */
    Q_PROPERTY(bool displayCents READ displayCents WRITE setDisplayCents NOTIFY displayCentsChanged FINAL)
public:
    explicit BitcoinValue(QObject *parent = nullptr);

    enum ValueSource {
        UserInput, // the result of things like insertNumnber() or paste()
        FromNumber // setValue() with a number was called.
    };

    qint64 value() const;
    void setValue(qint64 value, ValueSource source);

    inline double realValue() {
        return m_value;
    }
    inline void setRealValue(double val) {
        setValue(val, FromNumber);
    }

    Q_INVOKABLE void moveLeft();
    Q_INVOKABLE void moveRight();
    Q_INVOKABLE bool insertNumber(QChar number);
    Q_INVOKABLE bool addSeparator();
    Q_INVOKABLE void paste();
    Q_INVOKABLE bool backspacePressed();
    Q_INVOKABLE void reset();

    QString enteredString() const;

    int cursorPos() const;
    void setCursorPos(int newCursorPos);

    bool fiatMode() const;
    void setFiatMode(bool newFiatMode);

    bool displayCents() const;
    void setDisplayCents(bool newDisplayCents);

signals:
    void valueChanged();
    void enteredStringChanged();
    void cursorPosChanged();
    void fiatModeChanged();
    void displayCentsChanged();

protected slots:
    void processNumberValue();

protected:
    // sets integer form of m_value from the \a value
    // returns false if the string is not a proper number (or out of bounds)
    bool setStringValue(const QString &value);

    qint64 m_value;
    QString m_typedNumber;
    int m_cursorPos = 0;
    ValueSource m_valueSource = UserInput;
    bool m_fiatMode = false;
    bool m_displayCents = true;
};

#endif
