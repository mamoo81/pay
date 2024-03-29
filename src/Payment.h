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
#ifndef PAYMENT_H
#define PAYMENT_H


#include <QObject>
#include <QSet>

#include <primitives/Tx.h>
#include <memory>

class Wallet;
class PaymentDetail;
class PaymentDetailOutput;
class PaymentDetailInputs;
class AccountInfo;
class TxInfoObject;


class Payment : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int feePerByte READ feePerByte WRITE setFeePerByte NOTIFY feePerByteChanged)
    /// The payment-wide amount of funds being sent by this Payment.
    Q_PROPERTY(double paymentAmount READ paymentAmount WRITE setPaymentAmount NOTIFY amountChanged)
    /// The payment-wide amount of funds being sent by this Payment.
    Q_PROPERTY(int paymentAmountFiat READ paymentAmountFiat WRITE setPaymentAmountFiat NOTIFY amountFiatChanged)
    /// The single-output address we will send to
    Q_PROPERTY(QString targetAddress READ targetAddress WRITE setTargetAddress NOTIFY targetAddressChanged)
    // cleaned up and re-formatted
    Q_PROPERTY(QString formattedTargetAddress READ formattedTargetAddress NOTIFY targetAddressChanged)
    // cleaned but short.
    Q_PROPERTY(QString niceAddress READ niceAddress NOTIFY targetAddressChanged)
    Q_PROPERTY(bool preferSchnorr READ preferSchnorr WRITE setPreferSchnorr NOTIFY preferSchnorrChanged)
    /// If input is valid, tx can be prepared. \see prepare()
    Q_PROPERTY(bool isValid READ validate NOTIFY validChanged)
    Q_PROPERTY(QList<QObject*> details READ paymentDetails NOTIFY paymentDetailsChanged)
    Q_PROPERTY(BroadcastStatus broadcastStatus  READ broadcastStatus NOTIFY broadcastStatusChanged)
    /// The price of on BCH
    Q_PROPERTY(int fiatPrice READ fiatPrice WRITE setFiatPrice NOTIFY fiatPriceChanged)
    Q_PROPERTY(AccountInfo *account READ currentAccount WRITE setCurrentAccount NOTIFY currentAccountChanged)
    Q_PROPERTY(QString userComment READ userComment WRITE setUserComment NOTIFY userCommentChanged)
    Q_PROPERTY(bool walletNeedsPin READ walletNeedsPin NOTIFY walletPinChanged)
    Q_PROPERTY(bool autoPrepare READ autoPrepare WRITE setAutoPrepare NOTIFY autoPrepareChanged)
    Q_PROPERTY(bool instaPay READ allowInstaPay WRITE setAllowInstaPay NOTIFY allowInstaPayChanged)
    Q_PROPERTY(bool simpleAddressTarget READ simpleAddressTarget NOTIFY simpleAddressTargetChanged FINAL)

    // --- Stuff that becomes available / useful after prepare has been called:
    /// Tx has been prepared
    Q_PROPERTY(bool txPrepared READ txPrepared NOTIFY txPreparedChanged)
    Q_PROPERTY(QString txid READ txid NOTIFY txCreated)
    Q_PROPERTY(int assignedFee READ assignedFee NOTIFY txCreated)
    Q_PROPERTY(int txSize READ txSize NOTIFY txCreated)
    /// If prepare() failed, this is set.
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

    Q_PROPERTY(QStringList warnings READ warnings NOTIFY warningsChanged)


    Q_ENUMS(DetailType BroadcastStatus)
public:
    enum DetailType {
        InputSelector,
        PayToAddress
    };

    /**
     * The broadcast status is a statemachine to indicate if the transaction
     * has been offered to the network to the final state of
     * either TxRejected or TxBroadcastSuccess
     *
     * The statemachine goes like this;
     *
     * 0. `NotStarted`
     * 1. After the API call 'broadcast()' we offer the transaction to all peers.
     *    `TxOffered`
     * 2. A peer responds by downloading the actual transaction from us.
     *    `TxWaiting`
     * 3. Optionally, a peer responds with 'rejected' if the transaction is somehow wrong.
     *    `TxRejected`
     *    Stop here.
     * 4. We waited a little time and no rejected came in, implying 2 or more peers like our tx.
     *    `TxBroadcastSuccess`
     */
    enum BroadcastStatus {
        NotStarted,     //< We have not yet seen a call to broadcast()
        TxOffered,      //< Tx has not been offered to any peers.
        TxSent1,        //< Tx has been sent to at least one peer.
        TxWaiting,      //< Tx has been downloaded by more than one peer.
        TxBroadcastSuccess, //< Tx broadcast and accepted by multiple peers.
        TxRejected      //< Tx has been offered, downloaded and rejected by at least one peer.
    };

    enum Warning {
        InsecureTransport,
        DownloadFailed,
        OfflineWarning
    };
    Q_ENUM(Warning);

    Payment(QObject *parent = nullptr);

    void setFeePerByte(int sats);
    int feePerByte();

    /**
     * Sats the amount of BCH on our single-detail payment.
     *
     * This method can no longer be used after addExtraOutput() have been called.
     * This method assumes a single output, which is the default for this class.
     *
     * This sets the amount to pay, in Satoshis, to the target address.
     */
    void setPaymentAmount(double amount);
    /**
     * Returns the total amount of satoshis that are selected by outputs.
     */
    double paymentAmount() const;

    void setPaymentAmountFiat(int amount);
    int paymentAmountFiat() const;

    /**
     * Sets the address to pay to.
     *
     * This method can no longer be used after addExtraOutput has been called.
     * This method assumes a single output, which is the default for this class.
     * @see pasteTargetAddress()
     */
    void setTargetAddress(const QString &address);

    /**
     * Returns the address to pay to, as the user typed it.
     *
     * This method can no longer be used after addExtraOutput has been called.
     * This method assumes a single output, which is the default for this class.
     */
    QString targetAddress();
    /**
     * Returns the validated and formatted address to pay to.
     *
     * This method can no longer be used after addExtraOutput has been called.
     * This method assumes a single output, which is the default for this class.
     */
    QString formattedTargetAddress() const;
    /**
     * The nicest to display address version.
     * This will always return (if available) the BCH style (cash-address) address, without
     * the bitcoincash: prefix.
     */
    QString niceAddress() const;

    bool walletNeedsPin() const;

    /// return true if all fields are correctly populated and we can prepare()
    bool validate();
    /// A user error occured during prepare()
    const QString &error() const;

    Q_INVOKABLE void prepare();
    Q_INVOKABLE void markUserApproved();
    Q_INVOKABLE void reset();
    Q_INVOKABLE PaymentDetail* addExtraOutput();
    Q_INVOKABLE PaymentDetail* addInputSelector();
    Q_INVOKABLE void remove(PaymentDetail *detail);
    /**
     * Set a should-be-correct address and return true if valid.
     *
     * Calling thie function is very similar to setting the property 'targetAddress'
     * with the main difference that should the address pasted not be a valid one,
     * this method returns false.
     * @see setTargetAddress
     */
    Q_INVOKABLE bool pasteTargetAddress(const QString &address);
    /**
     * Unlock the account in order to allow funding of this payment.
     */
    Q_INVOKABLE void decrypt(const QString &password);

    /// return the txid, should there be a transaction (otherwise empty string)
    QString txid() const;

    /// The fee decided to be used in 'prepare()'.
    int assignedFee() const;
    /// The size of the transaction we prepare()d.
    int txSize() const;
    /// Return true if prepare() successfully completed.
    bool txPrepared() const;

    /**
     * This returns a total fiat amount input into the prepared transaction.
     */
    int effectiveFiatAmount() const;
    /**
     * This returns a total BCH (in sats) amount that went into the prepared transaction.
     */
    double effectiveBchAmount() const;

    /// Return the wallet used by the previous prepare()
    /// \sa currentAccount
    Wallet *wallet() const;

    bool preferSchnorr() const;
    void setPreferSchnorr(bool preferSchnorr);

    QList<QObject *> paymentDetails() const;

    BroadcastStatus broadcastStatus() const;

    /// The exchange rate. The amount of cents for one BCH.
    int fiatPrice() const;
    /// The exchange rate. The amount of cents for one BCH.
    void setFiatPrice(int pricePerCoin);

    AccountInfo *currentAccount() const;
    void setCurrentAccount(AccountInfo *account);

    const QString &userComment() const;
    void setUserComment(const QString &comment);

    bool autoPrepare() const;
    void setAutoPrepare(bool newAutoPrepare);

    bool allowInstaPay() const;
    void setAllowInstaPay(bool allowIt);

    Tx tx() const;

    /*
     * When an issue occurred with a consumer of this class and the error should be shown to the user.
     */
    void forwardError(const QString &error);

    /**
     * When an warning occurred with a consumer of this class and the warning should be shown to the user.
     */
    void addWarning(Warning warning);
    QStringList warnings() const;
    Q_INVOKABLE void clearWarnings();

    /// Bypass the broadcast mechanism and mark the transaction as received.
    void confirmReceivedOk();

    bool simpleAddressTarget() const;
    void setSimpleAddressTarget(bool newSimpleAddressTarget);

private slots:
    void recalcAmounts();
    void broadcast();

signals:
    void feePerByteChanged();
    void amountChanged();
    void amountFiatChanged();
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
    void userCommentChanged();
    void walletPinChanged();
    void autoPrepareChanged();
    void allowInstaPayChanged();
    void warningsChanged();
    void simpleAddressTargetChanged();
    void approvedByUser();

private:
    void doAutoPrepare();
    friend class PaymentDetailOutput;

    /// Helper method to get the output, assuming that is the only detail.
    /// Will throw if the Payment has more than one detail.
    PaymentDetailOutput *soleOut() const;
    void addDetail(PaymentDetail*);

    AccountInfo *m_account = nullptr;

    // Payment Variable initialization in reset() please
    QSet<Warning> m_warnings;
    QList<PaymentDetail*> m_paymentDetails;
    bool m_autoPrepare = false;
    bool m_txPrepared;
    bool m_txBroadcastStarted;
    bool m_preferSchnorr;
    bool m_allowInstaPay = false;
    bool m_simpleAddressTarget = true; // only 'advanced' payment protocols set this to false
    Tx m_tx;
    int m_fee; // in sats per byte
    int m_assignedFee;
    int m_fiatPrice = 50000; // price for one whole BCH
    std::shared_ptr<TxInfoObject> m_infoObject;
    Wallet *m_wallet = nullptr;
    QString m_error;
    QString m_userComment;
};


class PaymentDetail : public QObject
{
    Q_OBJECT
    Q_PROPERTY(Payment::DetailType type READ type CONSTANT)
    Q_PROPERTY(bool collapsable READ collapsable WRITE setCollapsable NOTIFY collapsableChanged)
    Q_PROPERTY(bool collapsed READ collapsed WRITE setCollapsed NOTIFY collapsedChanged)
    Q_PROPERTY(bool isOutput READ isOutput CONSTANT)
    Q_PROPERTY(bool isInput READ isInputs CONSTANT)
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

    /**
     * When the user selects another wallet (via Payment::setCurrentAccount)
     * default implementation is empty.
     */
    virtual void setWallet(Wallet *wallet);

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

#endif
