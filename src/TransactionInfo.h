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
#ifndef TRANSACTIONINFO_H
#define TRANSACTIONINFO_H

#include <QObject>
#include <QString>
#include <QList>
#include <QDateTime>

class Wallet;

class TransactionInputInfo : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString address READ address CONSTANT)
    Q_PROPERTY(QString cloakedAddress READ cloakedAddress CONSTANT)
    Q_PROPERTY(qint64 value READ value CONSTANT)
    Q_PROPERTY(bool fromFused READ fromFused CONSTANT FINAL)
public:
    TransactionInputInfo(QObject *parent);

    QString address() const;
    void setAddress(const QString &address);

    qint64 value() const;
    void setValue(const qint64 &value);

    const QString &cloakedAddress() const;
    void setCloakedAddress(const QString &newCloakedAddress);

    void setFromFused(bool fused);
    bool fromFused() const;

private:
    QString m_address;
    QString m_cloakedAddress;
    qint64 m_value = 0;
    bool m_fromFused = false;
};

class TransactionOutputInfo : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString address READ address CONSTANT)
    Q_PROPERTY(QString cloakedAddress READ cloakedAddress CONSTANT)
    Q_PROPERTY(qint64 value READ value CONSTANT)
    Q_PROPERTY(bool spent READ spent CONSTANT)
    Q_PROPERTY(bool forMe READ forMe CONSTANT)
    Q_PROPERTY(bool hasCashToken READ hasCashToken CONSTANT)
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

    bool hasCashToken() const;
    void setHasCashToken(bool ct);

private:
    QString m_address;
    QString m_cloakedAddress;
    qint64 m_value;
    bool m_spent = false;
    bool m_forMe = true;
    bool m_hasCashToken = false;
};

class TransactionInfo : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int size READ txSize CONSTANT)
    /**
     * Return the amount of fees paid for this transaction.
     * This returns zero if we didn't create the transaction.
     */
    Q_PROPERTY(double fees READ fees CONSTANT FINAL)
    Q_PROPERTY(QList<QObject*> inputs READ inputs CONSTANT FINAL)
    Q_PROPERTY(QList<QObject*> outputs READ outputs CONSTANT FINAL)
    Q_PROPERTY(QList<QObject*> knownInputs READ knownInputs CONSTANT FINAL)
    Q_PROPERTY(QList<QObject*> knownOutputs READ knownOutputs CONSTANT FINAL)
    Q_PROPERTY(QString userComment READ userComment WRITE setUserComment NOTIFY commentChanged FINAL)
    /**
     * The recipient of the transaction. Typically just an address.
     */
    Q_PROPERTY(QString receiver READ receiver CONSTANT FINAL)
    Q_PROPERTY(bool isCoinbase READ isCoinbase CONSTANT FINAL)
    Q_PROPERTY(bool isFused READ isFused CONSTANT FINAL)
    Q_PROPERTY(bool createdByUs READ createdByUs CONSTANT FINAL)
    Q_PROPERTY(bool commentEditable READ commentEditable CONSTANT FINAL)

    Q_PROPERTY(QString txid READ txid CONSTANT FINAL)
    Q_PROPERTY(int minedHeight READ minedHeight NOTIFY blockHeightChanged FINAL)
    Q_PROPERTY(QDateTime date READ transactionDate CONSTANT FINAL)
    Q_PROPERTY(double fundsIn READ fundsIn CONSTANT FINAL)
    Q_PROPERTY(double fundsOut READ fundsOut CONSTANT FINAL)

public:
    explicit TransactionInfo(QObject *parent = nullptr);

    int txSize() const;
    double fees() const;
    QList<QObject *> outputs() const;
    QList<QObject *> inputs() const;
    QList<QObject *> knownOutputs() const;
    QList<QObject *> knownInputs() const;
    const QString &userComment() const;
    void setUserComment(const QString &comment);
    bool isCoinbase() const;
    bool isFused() const;
    bool createdByUs() const;

    /// the combined amount of sats spent by us in the inputs
    double fundsIn() const;
    /// the combined amount of sats received by us in the outputs
    double fundsOut() const;

    /// the time we first saw this transaction.
    QDateTime transactionDate() const;

    /// the block-height this transaction got mined in.
    /// it will be -1 for unconfirmed, or -2 for rejected.
    int minedHeight() const;

    /// The 'receiver' address of this transaction.
    QString receiver() const;
    /// the transactionId, as string.
    QString txid() const;

    /**
     * The API allows the user to set a 'user comment'. Which needs
     * to be forwarded to the wallet to make that persistent.
     * Call this method with the appropriate details to make enable
     * editing of the user comment.
     */
    void setWalletDetails(Wallet *wallet, int walletIndex);
    bool commentEditable() const;

    int m_txSize = 0;
    QList<TransactionOutputInfo *> m_outputs;
    QList<TransactionInputInfo *> m_inputs;
    QString m_userComment;
    bool m_isCoinbase = false;
    bool m_isFused = false;
    bool m_createdByUs = false;

signals:
    void commentChanged();
    void blockHeightChanged();

private:
    Wallet *m_wallet = nullptr;
    int m_walletIndex = -1;
};

#endif
