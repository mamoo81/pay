/*
 * This file is part of the Flowee project
 * Copyright (C) 2020-2023 Tom Zander <tom@flowee.org>
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
#include "Wallet.h"

TransactionInfo::TransactionInfo(QObject *parent)
    : QObject(parent)
{
}

int TransactionInfo::txSize() const
{
    return m_txSize;
}

double TransactionInfo::fees() const
{
    qint64 fees = 0;
    if (m_createdByUs) {
        for (const auto i : m_inputs) {
            fees += i->value();
        }
        for (const auto o : m_outputs) {
            fees -= o->value();
        }
    }
    return static_cast<double>(fees);
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

void TransactionInfo::setUserComment(const QString &comment)
{
    if (comment == m_userComment)
        return;
    if (m_wallet)
        m_wallet->setTransactionComment(m_walletIndex, comment);
    m_userComment = comment;
    emit commentChanged();
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

QString TransactionInfo::receiver() const
{
    if (!m_createdByUs) // the receiver, is us!
        return QString();
    /*
     * We iterate over the outputs and reject all outputs that we own.
     * If exactly one is left we have a 'receiver'.
     */
    QString answer;
    for (auto out : m_outputs) {
        if (!out->forMe()) {
            if (answer.isEmpty())
                answer = out->address();
            else
                return QString(); // more than one address
        }

    }
    return answer;
}

void TransactionInfo::setWalletDetails(Wallet *wallet, int walletIndex)
{
    m_wallet = wallet;
    m_walletIndex = walletIndex;
}

bool TransactionInfo::commentEditable() const
{
    return m_wallet;
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
