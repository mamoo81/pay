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
#include "TransactionInfo.h"

TransactionInfo::TransactionInfo(QObject *parent)
    : QObject(parent)
{
}

int TransactionInfo::txSize() const
{
    return m_txSize;
}

QList<QObject *> TransactionInfo::outputs() const
{
    QList<QObject *> answer;
    for (auto o : m_outputs) {
        answer.append(o);
    }
    return answer;
}

QList<QObject *> TransactionInfo::inputs() const
{
    QList<QObject *> answer;
    for (auto i : m_inputs) {
        answer.append(i);
    }
    return answer;
}

const QString &TransactionInfo::userComment() const
{
    return m_userComment;
}

bool TransactionInfo::isCashFusion() const
{
    return m_isCashFusion;
}

bool TransactionInfo::isCoinbase() const
{
    return m_isCoinbase;
}

bool TransactionInfo::createdByUs() const
{
    return m_createdByUs;
}


// //////////////////////////////////////////////////////

TransactionInputInfo::TransactionInputInfo(QObject *parent)
    : QObject(parent)
{
}

QString TransactionInputInfo::address() const
{
    return m_address;
}

void TransactionInputInfo::setAddress(const QString &address)
{
    m_address = address;
}

qint64 TransactionInputInfo::value() const
{
    return m_value;
}

void TransactionInputInfo::setValue(const qint64 &value)
{
    m_value = value;
}

const QString &TransactionInputInfo::cloakedAddress() const
{
    return m_cloakedAddress;
}

void TransactionInputInfo::setCloakedAddress(const QString &newCloakedAddress)
{
    m_cloakedAddress = newCloakedAddress;
}


// //////////////////////////////////////////////////////

QString TransactionOutputInfo::address() const
{
    return m_address;
}

void TransactionOutputInfo::setAddress(const QString &address)
{
    m_address = address;
}

qint64 TransactionOutputInfo::value() const
{
    return m_value;
}

void TransactionOutputInfo::setValue(const qint64 &value)
{
    m_value = value;
}

bool TransactionOutputInfo::spent() const
{
    return m_spent;
}

void TransactionOutputInfo::setSpent(bool spent)
{
    m_spent = spent;
}

bool TransactionOutputInfo::forMe() const
{
    return m_forMe;
}

void TransactionOutputInfo::setForMe(bool forMe)
{
    m_forMe = forMe;
}

const QString &TransactionOutputInfo::cloakedAddress() const
{
    return m_cloakedAddress;
}

void TransactionOutputInfo::setCloakedAddress(const QString &ad)
{
    m_cloakedAddress = ad;
}
