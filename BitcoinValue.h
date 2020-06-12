/*
 * This file is part of the Flowee project
 * Copyright (C) 2020 Tom Zander <tomz@freedommail.ch>
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
    Q_PROPERTY(QString strValue READ stringForValue WRITE setStringValue NOTIFY valueChanged)
    Q_PROPERTY(QString enteredString READ enteredString WRITE setEnteredString NOTIFY enteredStringChanged)
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

    Q_INVOKABLE void addNumber(QChar number);
    Q_INVOKABLE void addSeparator();
    Q_INVOKABLE void backspacePressed();

    QString stringForValue() const;
    void setStringValue(const QString &value);

    inline QString enteredString() const {
        return m_typedNumber;
    }
    void setEnteredString(const QString &s);

signals:
    void valueChanged();
    void enteredStringChanged();

private:
    qint64 m_value;
    QString m_typedNumber;
};

#endif
