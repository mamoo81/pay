/*
 * This file is part of the Flowee project
 * Copyright (C) 2020-2024 Tom Zander <tom@flowee.org>
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
            /*
             * We may assume that if a transaction is created by us that this
             * means we have all inputs. An assumption that makes sense.
             * It does hit the snag that if a user started an import at a later date
             * than the input was created that we don't actually have it. We should, but
             * we don't.
             *
             * So, make sure to check our pointers here.
             */
            if (i)
                fees += i->value();
        }
        for (const auto o : m_outputs) {
            if (o)
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

QList<QObject *> TransactionInfo::knownOutputs() const
{
    QList<QObject *> answer;
    for (auto o : m_outputs) {
        if (o) answer.append(o);
    }
    return answer;
}

QList<QObject *> TransactionInfo::knownInputs() const
{
    QList<QObject *> answer;
    for (auto i : m_inputs) {
        if (i) answer.append(i);
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

bool TransactionInfo::isFused() const
{
    return m_isFused;
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
    QString answer;
    /*
     * We iterate over the outputs and reject all outputs that make no sense.
     * If exactly one is left we have a 'receiver'.
     */
    for (auto out : m_outputs) {
        if (out && out->forMe() != m_createdByUs) {
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

void TransactionInputInfo::setFromFused(bool fused)
{
    m_fromFused = fused;
}

bool TransactionInputInfo::fromFused() const
{
    return m_fromFused;
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

bool TransactionOutputInfo::hasCashToken() const
{
    return m_hasCashToken;
}

void TransactionOutputInfo::setHasCashToken(bool ct)
{
    m_hasCashToken = ct;
}
