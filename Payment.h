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

#include "WalletCoinsModel.h"

#include <BroadcastTxData.h>
#include <primitives/Tx.h>
#include <memory>

class Wallet;
class PaymentDetail;
class PaymentDetailOutput;
class PaymentDetailInputs;
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
    /// If Prepare failed, this is set.
    Q_PROPERTY(QString error READ error NOTIFY errorChanged);
    /// Tx has been prepared
    Q_PROPERTY(bool txPrepared READ txPrepared NOTIFY txPreparedChanged);
    Q_PROPERTY(bool preferSchnorr READ preferSchnorr WRITE setPreferSchnorr NOTIFY preferSchnorrChanged);
    /// Input is valid, tx can be prepared
    Q_PROPERTY(bool isValid READ validate NOTIFY validChanged);
    Q_PROPERTY(QList<QObject*> details READ paymentDetails NOTIFY paymentDetailsChanged);
    Q_PROPERTY(BroadcastStatus broadcastStatus  READ broadcastStatus NOTIFY broadcastStatusChanged)
    /// The price of on BCH
    Q_PROPERTY(int fiatPrice READ fiatPrice WRITE setFiatPrice NOTIFY fiatPriceChanged)
    Q_PROPERTY(AccountInfo *account READ currentAccount WRITE setCurrentAccount NOTIFY currentAccountChanged)

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

    /// return true if all fields are correctly populated and we can prepare()
    bool validate();
    /// A user error occured during prepare()
    const QString &error() const;

    Q_INVOKABLE void prepare();
    Q_INVOKABLE void broadcast();
    Q_INVOKABLE void reset();
    Q_INVOKABLE void addExtraOutput();
    Q_INVOKABLE void addInputSelector();
    Q_INVOKABLE void remove(PaymentDetail *detail);

    /// return the txid, should there be a transaction (otherwise empty string)
    QString txid() const;

    int assignedFee() const;
    int txSize() const;
    bool txPrepared() const;

    /// Return the wallet used by the previous prepare()
    /// \sa currentAccount
    Wallet *wallet() const;

    bool preferSchnorr() const;
    void setPreferSchnorr(bool preferSchnorr);

    QList<QObject *> paymentDetails() const;

    BroadcastStatus broadcastStatus() const;

    int fiatPrice() const;
    void setFiatPrice(int newFiatPrice);

    AccountInfo *currentAccount() const;
    void setCurrentAccount(AccountInfo *account);

private slots:
    void sentToPeer();
    void txRejected(short reason, const QString &message);
    void paymentAmountChanged();

signals:
    void feePerByteChanged();
    void amountChanged();
    void targetAddressChanged();
    void txPreparedChanged();
    void preferSchnorrChanged();
    void paymentDetailsChanged();
    void broadcastStatusChanged();
    void validChanged();
    void errorChanged();
    void txCreated();
    void fiatPriceChanged();
    void currentAccountChanged();

private:
    friend class PaymentDetailOutput;

    /// Helper method to get the output, assuming that is the only detail.
    /// Will throw if the Payment has more than one detail.
    PaymentDetailOutput *soleOut() const;
    void addDetail(PaymentDetail*);

    AccountInfo *m_account = nullptr;

    // Payment Variable initialization in reset() please
    QList<PaymentDetail*> m_paymentDetails;
    bool m_txPrepared;
    bool m_preferSchnorr;
    Tx m_tx;
    int m_fee; // in sats per byte
    int m_assignedFee;
    int m_fiatPrice; // price for one whole BCH
    std::shared_ptr<BroadcastTxData> m_infoObject;
    short m_sentPeerCount;
    short m_rejectedPeerCount;
    Wallet *m_wallet;
    QString m_error;
};


class PaymentDetail : public QObject
{
    Q_OBJECT
    Q_PROPERTY(Payment::DetailType type READ type CONSTANT)
    Q_PROPERTY(bool collapsable READ collapsable WRITE setCollapsable NOTIFY collapsableChanged)
    Q_PROPERTY(bool collapsed READ collapsed WRITE setCollapsed NOTIFY collapsedChanged)
public:
    PaymentDetail(Payment *parent, Payment::DetailType type);


    Payment::DetailType type() const;

    bool collapsable() const;
    void setCollapsable(bool newCollapsable);

    bool collapsed() const;
    void setCollapsed(bool newCollapsed);

    inline bool isOutput() const {
        return m_type == Payment::PayToAddress;
    }
    inline bool isInputs() const {
        return m_type == Payment::InputSelector;
    }
    PaymentDetailOutput *toOutput();
    PaymentDetailInputs *toInputs();

    bool valid() const;

protected:
    void setValid(bool valid);

signals:
    void collapsableChanged();
    void collapsedChanged();
    void validChanged();

    void maxAllowedChanged();

private:
    const Payment::DetailType m_type;
    bool m_collapsable = true;
    bool m_collapsed = false;
    bool m_valid = false; // when all user-input is valid
};

#endif
