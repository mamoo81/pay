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
#ifndef PAYMENT_H
#define PAYMENT_H

#include <QObject>
#include <BroadcastTxData.h>
#include <primitives/Tx.h>
#include <memory>

class Wallet;
class PaymentDetail;
class PaymentDetailOutput;
class AccountInfo;

class Payment : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int feePerByte READ feePerByte WRITE setFeePerByte NOTIFY feePerByteChanged)
    Q_PROPERTY(double paymentAmount READ paymentAmount WRITE setPaymentAmount NOTIFY amountChanged)
    Q_PROPERTY(QString targetAddress READ targetAddress WRITE setTargetAddress NOTIFY targetAddressChanged)
    // cleaned up and re-formatted
    Q_PROPERTY(QString formattedTargetAddress READ formattedTargetAddress NOTIFY targetAddressChanged)
    Q_PROPERTY(QString txid READ txid NOTIFY txCreated)
    Q_PROPERTY(int assignedFee READ assignedFee NOTIFY txCreated)
    Q_PROPERTY(int txSize READ txSize NOTIFY txCreated)
    /// Tx has been prepared
    Q_PROPERTY(bool walletOk READ walletOk NOTIFY walletOkChanged);
    Q_PROPERTY(bool txPrepared READ txPrepared NOTIFY txPreparedChanged);
    Q_PROPERTY(bool preferSchnorr READ preferSchnorr WRITE setPreferSchnorr NOTIFY preferSchnorrChanged);
    /// Input is valid, tx can be prepared
    Q_PROPERTY(bool isValid READ validate NOTIFY validChanged);
    Q_PROPERTY(QList<QObject*> details READ paymentDetails NOTIFY paymentDetailsChanged);
    Q_PROPERTY(BroadcastStatus broadcastStatus  READ broadcastStatus NOTIFY broadcastStatusChanged)

    Q_ENUMS(DetailType BroadcastStatus)
public:
    enum DetailType {
        InputSelector,
        PayToAddress
    };

    enum BroadcastStatus {
        TxNotSent,
        TxRejected,
        TxWaiting,
        TxSent1,
        TxBroadcastSuccess
    };

    Payment(QObject *parent = nullptr);

    void setFeePerByte(int sats);
    int feePerByte();

    /**
     * Set the amount we want to have arrive on the other side.
     * Special value can be -1 to indicate all available outputs.
     */
    void setPaymentAmount(double amount);
    double paymentAmount();

    void setTargetAddress(const QString &address);
    QString targetAddress();
    QString formattedTargetAddress();

    Q_INVOKABLE bool validate();
    Q_INVOKABLE void prepare(AccountInfo *currentAccount);
    Q_INVOKABLE void broadcast();
    Q_INVOKABLE void reset();

    Q_INVOKABLE PaymentDetail* addExtraOutput();

    /// return the txid, should there be a transaction (otherwise empty string)
    QString txid() const;

    int assignedFee() const;
    int txSize() const;
    bool txPrepared() const;

    Wallet *wallet() const;
    /// returns false if the wallet failed to supply funds during prepare()
    bool walletOk() const {
        return m_walletOk;
    }

    bool preferSchnorr() const;
    void setPreferSchnorr(bool preferSchnorr);

    QList<QObject *> paymentDetails() const;

    BroadcastStatus broadcastStatus() const;

private slots:
    void sentToPeer();
    void txRejected(short reason, const QString &message);

signals:
    void feePerByteChanged();
    void amountChanged();
    void targetAddressChanged();
    void txPreparedChanged();
    void preferSchnorrChanged();
    void paymentDetailsChanged();
    void broadcastStatusChanged();
    void validChanged();
    void walletOkChanged();
    void txCreated();

private:
    /// Helper method to get the output, assuming that is the only detail.
    /// Will throw if the Payment has more than one detail.
    PaymentDetailOutput *soleOut() const;
    PaymentDetail *addDetail(PaymentDetail*);

    // Variable initialization in reset() please
    Wallet *m_wallet;
    QList<PaymentDetail*> m_paymentDetails;
    bool m_txPrepared;
    bool m_preferSchnorr;
    bool m_walletOk;
    Tx m_tx;
    int m_fee; // in sats per byte
    int m_assignedFee;
    std::shared_ptr<BroadcastTxData> m_infoObject;
    short m_sentPeerCount;
    short m_rejectedPeerCount;
};

class PaymentDetail : public QObject
{
    Q_OBJECT
    Q_PROPERTY(Payment::DetailType type READ type CONSTANT)
    Q_PROPERTY(bool collapsable READ collapsable WRITE setCollapsable NOTIFY collapsableChanged)
    Q_PROPERTY(bool collapsed READ collapsed WRITE setCollapsed NOTIFY collapsedChanged)
public:
    PaymentDetail(Payment::DetailType type, QObject *parent = nullptr);


    Payment::DetailType type() const;

    bool collapsable() const;
    void setCollapsable(bool newCollapsable);

    bool collapsed() const;
    void setCollapsed(bool newCollapsed);

    PaymentDetailOutput *toOutput();

protected:
    void setValid(bool valid);

signals:
    void collapsableChanged();
    void collapsedChanged();
    void validChanged();

private:
    const Payment::DetailType m_type;
    bool m_collapsable = true;
    bool m_collapsed = false;
    bool m_valid = false; // when all user-input is valid
};

class PaymentDetailOutput : public PaymentDetail
{
    Q_OBJECT
    Q_PROPERTY(double paymentAmount READ paymentAmount WRITE setPaymentAmount NOTIFY paymentAmountChanged)
    Q_PROPERTY(QString address READ address WRITE setAddress NOTIFY addressChanged)
    // cleaned up and re-formatted
    Q_PROPERTY(QString formattedTarget READ formattedTarget NOTIFY addressChanged)
public:
    PaymentDetailOutput(QObject *parent = nullptr);

    qint64 amount() const {
        return m_paymentAmount;
    }
    double paymentAmount() const;
    void setPaymentAmount(double newPaymentAmount);

    /// this method also sets formattedTarget if its a proper address.
    /// @see FloweePay::identifyString()
    const QString &address() const;
    void setAddress(const QString &newAddress);
    /// is non-empty if the address() is proper.
    const QString &formattedTarget() const;

signals:
    void paymentAmountChanged();
    void addressChanged();

private:
    void checkValid();

    qint64 m_paymentAmount = 0;
    QString m_address;
    QString m_formattedTarget;
};

class PaymentDetailInputs : public PaymentDetail
{
    Q_OBJECT
public:
    PaymentDetailInputs(QObject *parent = nullptr);

};

inline PaymentDetailOutput *PaymentDetail::toOutput() {
    assert(m_type == Payment::PayToAddress);
    return static_cast<PaymentDetailOutput*>(this);
}

#endif
