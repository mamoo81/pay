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
    Q_PROPERTY(QString cloakedAddress READ cloakedAddress CONSTANT)
    Q_PROPERTY(qint64 value READ value CONSTANT)
public:
    TransactionInputInfo(QObject *parent);

    QString address() const;
    void setAddress(const QString &address);

    qint64 value() const;
    void setValue(const qint64 &value);

    const QString &cloakedAddress() const;
    void setCloakedAddress(const QString &newCloakedAddress);

private:
    QString m_address;
    QString m_cloakedAddress;
    qint64 m_value = 0;
};

class TransactionOutputInfo : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString address READ address CONSTANT)
    Q_PROPERTY(QString cloakedAddress READ cloakedAddress CONSTANT)
    Q_PROPERTY(qint64 value READ value CONSTANT)
    Q_PROPERTY(bool spent READ spent CONSTANT)
    Q_PROPERTY(bool forMe READ forMe CONSTANT)
public:
    TransactionOutputInfo(QObject *parent) : QObject(parent) {}
    QString address() const;
    void setAddress(const QString &address);

    qint64 value() const;
    void setValue(const qint64 &value);

    bool spent() const;
    void setSpent(bool spent);

    /// output matches an address owned by the wallet.
    bool forMe() const;
    void setForMe(bool forMe);

    const QString &cloakedAddress() const;
    void setCloakedAddress(const QString &ad);

private:
    QString m_address;
    QString m_cloakedAddress;
    qint64 m_value;
    bool m_spent = false;
    bool m_forMe = true;
};

class TransactionInfo : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int size READ txSize CONSTANT)
    Q_PROPERTY(QList<QObject*> inputs READ inputs CONSTANT)
    Q_PROPERTY(QList<QObject*> outputs READ outputs CONSTANT)
    Q_PROPERTY(QString userComment READ userComment CONSTANT)
    Q_PROPERTY(bool isCoinbase READ isCoinbase CONSTANT)
    Q_PROPERTY(bool isCashFusion READ isCashFusion CONSTANT)
    Q_PROPERTY(bool createdByUs READ createdByUs CONSTANT)

public:
    explicit TransactionInfo(QObject *parent = nullptr);

    int txSize() const;
    QList<QObject *> outputs() const;
    QList<QObject *> inputs() const;
    const QString &userComment() const;
    bool isCoinbase() const;
    bool isCashFusion() const;
    bool createdByUs() const;

    int m_txSize = 0;
    QList<TransactionOutputInfo *> m_outputs;
    QList<TransactionInputInfo *> m_inputs;
    QString m_userComment;
    bool m_isCoinbase = false;
    bool m_isCashFusion = false;
    bool m_createdByUs = false;
};

#endif
