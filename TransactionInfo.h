/*
 * This file is part of the Flowee project
 * Copyright (C) 2020 Tom Zander <tom@flowee.org>
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
#ifndef TRANSACTIONINFO_H
#define TRANSACTIONINFO_H

#include <QObject>
#include <QString>
#include <QList>

class TransactionInputInfo : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString address READ address CONSTANT)
    Q_PROPERTY(qint64 value READ value CONSTANT)
public:
    TransactionInputInfo(QObject *parent) : QObject(parent) {}
    QString address() const;
    void setAddress(const QString &address);

    qint64 value() const;
    void setValue(const qint64 &value);

private:
    QString m_address;
    qint64 m_value;
    // int m_prevTxIndex;
};

class TransactionOutputInfo : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString address READ address CONSTANT)
    Q_PROPERTY(qint64 value READ value CONSTANT)
    Q_PROPERTY(bool spent READ spent CONSTANT)
public:
    TransactionOutputInfo(QObject *parent) : QObject(parent) {}
    QString address() const;
    void setAddress(const QString &address);

    qint64 value() const;
    void setValue(const qint64 &value);

    bool spent() const;
    void setSpent(bool spent);

private:
    QString m_address;
    qint64 m_value;
    bool m_spent;
};

class TransactionInfo : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int size READ txSize CONSTANT)
    Q_PROPERTY(QList<QObject*> inputs READ inputs CONSTANT)
    Q_PROPERTY(QList<QObject*> outputs READ outputs CONSTANT)
public:
    explicit TransactionInfo(QObject *parent = nullptr);

    int txSize() const;
    QList<QObject *> outputs() const;
    QList<QObject *> inputs() const;

    int m_txSize = 0;
    QList<TransactionOutputInfo *> m_outputs;
    QList<TransactionInputInfo *> m_inputs;
};

#endif
