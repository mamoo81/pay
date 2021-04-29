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
#ifndef FLOWEE_WALLET_H
#define FLOWEE_WALLET_H

#include <DataListenerInterface.h>
#include <PrivacySegment.h>

#include <primitives/key.h>
#include <primitives/pubkey.h>
#include <primitives/FastTransaction.h>

#include <boost/unordered_map.hpp>
#include <boost/filesystem.hpp>

#include <QMutex>
#include <QString>
#include <QObject>

class WalletInfoObject;
class TransactionInfo;
class PaymentRequest;

namespace P2PNet {
struct Notification;
}

class Wallet : public QObject, public DataListenerInterface
{
    Q_OBJECT
public:
    /**
     * Create an empty, new, wallet.
     */
    static Wallet *createWallet(const boost::filesystem::path &basedir, uint16_t segmentId, const QString &name = QString());

    /**
     * Load existing wallet.
     * This throws should there not be any data found.
     */
    Wallet(const boost::filesystem::path &basedir, uint16_t segmentId);
    ~Wallet();

    class OutputRef {
    public:
        explicit OutputRef(int txIndex = 0, int outputIndex = 0); // invalid
        explicit OutputRef(uint64_t encoded);
        OutputRef(const OutputRef &output) = default;
        uint64_t encoded() const;

        inline int txIndex() const {
            return m_txid;
        }
        inline int outputIndex() const {
            return m_outputIndex;
        }

        inline void setTxIndex(int index) {
            m_txid = index;
        }
        inline void setOutputIndex(int index) {
            m_outputIndex = index;
        }

    private:
        uint32_t m_txid = 0; // index in m_walletTransactions
        uint16_t m_outputIndex = 0;
    };

    /**
     * @brief newTransactions announces a list of transactions pushed to us from a peer.
     * @param header the block header these transactions appeared in.
     * @param blockHeight the blockheight we know the header under.
     * @param blockTransactions The actual transactions.
     */
    void newTransactions(const BlockHeader &header, int blockHeight, const std::deque<Tx> &blockTransactions) override;
    // notify about unconfirmed Tx.
    void newTransaction(const Tx &tx) override;
    /// Let the wallet know that it is up-to-date to \a height
    void setLastSynchedBlockHeight(int height) override;

    PrivacySegment *segment() const;

    /// Create a new private key.
    void createNewPrivateKey(int currentBlockheight);
    /// import an existing private key.
    bool addPrivateKey(const QString &privKey, int startBlockHeight);
    /// Save changed in historical wallet
    void saveWallet();

    qint64 balanceConfirmed() const {
        return m_balanceConfirmed;
    }
    qint64 balanceImmature() const {
        return m_balanceImmature;
    }
    qint64 balanceUnconfirmed() const {
        return m_balanceUnconfirmed;
    }

    /// return the amount of UTXOs that hold money
    int unspentOutputCount() const;
    /// return the amount of UTXOs ever created for this account.
    int historicalOutputCount() const;

    /// the user-visible name for the wallet.
    QString name() const;
    /// set the user-visible name for the wallet.
    void setName(const QString &name);

    /// Fetch UTXO txid
    inline const uint256 &txid(OutputRef ref) const {
        return txid(ref.txIndex());
    }
    /// Fetch UTXO txid
    const uint256 &txid(int txIndex) const;
    /// Fetch UTXO output
    Tx::Output txOutout(OutputRef ref) const;
    /// Fetch UTXO key
    const CKey &unlockKey(OutputRef ref) const;

    /// Return a bitcoin address (160 bits ripe key) for deposit.
    CKeyID nextUnusedAddress();

    /// Return a private-key-index for deposits and reserve it from re-use.
    int reserveUnusedAddress(CKeyID &keyId);

    /// The opposite of reserveUnusedAddress, free for usage an address.
    void unreserveAddress(int index);

    struct OutputSet {
        std::vector<OutputRef> outputs;
        qint64 totalSats = 0;
        int fee = 0;
    };
    /**
     * @brief findInputsFor UTXO fulfilment algo finding the inputs for your tx.
     * @param output The amount of satoshis you want to make available (after fee).
     *          A special value of -1 indicates all outputs.
     * @param feePerByte fee per byte
     * @param txSize the size of the transaction before we add inputs
     * @param[out] change the amount of satoshis we over-provided for the expected \a output.
     * @return The references to the outputs we suggest you fund your tx with.
     */
    OutputSet findInputsFor(qint64 output, int feePerByte, int txSize, int64_t &change) const;

    bool isSingleAddressWallet() const;
    void setSingleAddressWallet(bool isSingleAddressWallet);

    void fetchTransactionInfo(TransactionInfo *info, int txIndex);

    void addPaymentRequest(PaymentRequest *pr);
    void removePaymentRequest(PaymentRequest *pr);

    QList<PaymentRequest *> paymentRequests() const;

#ifdef IN_TESTS
    /**
     * Unit tests can call this to populate a wallet with some dummy transactions.
     */
    void addTestTransactions();
#endif

    /**
     * WHen true, then this wallet is the result of a user-initiated action.
     * Otherwise it was created by the system for some reason and is empty.
     */
    bool userOwnedWallet() const;
    /**
     * Changes a wallet to be user-owned or not.
     */
    void setUserOwnedWallet(bool userOwnedWallet);

private slots:
    void broadcastTxFinished(int txIndex, bool success);
    /// find all not-yet-confirmed transactions and start a broadcast
    void broadcastUnconfirmed();
    void recalculateBalance();

signals:
    void balanceChanged();
    void utxosChanged();
    void appendedTransactions(int firstNew, int count);
    void lastBlockSynchedChanged();
    void paymentRequestsChanged();
    void userOwnedChanged();

    void transactionConfirmed(int txIndex);

protected:
    Wallet();

private:
    /// see also saveWallet()
    void loadWallet();
    void loadSecrets();
    void saveSecrets();

    /**
     * Pass in an outputscript (scriptPubKey) and return the
     * wallet-secret ID that can unlock it.
     *
     * @return the wallet-secret-id or -1 if none match.
     */
    int findSecretFor(const Streaming::ConstBuffer &outputScript) const;

    /// Fill the bloom filter with all the unspent transactions and addresses we handle.
    void rebuildBloom();

    std::unique_ptr<PrivacySegment> m_segment;
    mutable QMutex m_lock;
    /// used to determine if we need to persist the wallet
    bool m_walletChanged = false;
    /// used to determine if we need to persist the private keys
    bool m_secretsChanged = false;

    struct WalletSecret {
        CKey privKey;
        CKeyID address;
        int initialHeight = -1;
        /// if true, this address has been reseved to receive funds on
        bool reserved = false; // in-mem-only
    };

    int m_nextWalletSecretId = 1;
    int m_nextWalletTransactionId = 1;
    int m_lastBlockHeightSeen = 0;
    short m_bloomScore = 0;
    std::map<int, WalletSecret> m_walletSecrets;

    QString m_name;

    struct Output {
        int walletSecretId = -1;
        uint64_t value = 0;
    };

    struct WalletTransaction {
        uint256 txid;
        uint256 minedBlock;
        int minedBlockHeight = 0;
        bool isCoinbase = false;
        QString userComment;

        // One entry for inputs that spent outputs in this wallet.
        // The key is the input. They value is a composition of the output-index (lower 2 bytes)
        // and the int-key in m_walletTransactions is the middle 4 bytes.
        std::map<int, uint64_t> inputToWTX;
        std::map<int, Output> outputs; // output-index to Output (only ones we can/could spend)
    };

    WalletTransaction createWalletTransactionFromTx(const Tx &tx, const uint256 &txid, P2PNet::Notification *change = nullptr) const;
    void saveTransaction(const Tx &tx);
    /**
     * returns a Tx if this txid was saved in this wallet.
     *
     * pool.reserve() is called by this method with the actual tx size.
     */
    Tx loadTransaction(const uint256 &txid, Streaming::BufferPool &pool) const;

    std::map<int, WalletTransaction> m_walletTransactions;

    typedef boost::unordered_map<uint256, int, HashShortener> TxIdCash;
    TxIdCash m_txidCash; // txid -> m_walletTransactions-id

    // cache
    std::map<uint64_t, uint64_t> m_unspentOutputs; // composited output -> value (in sat).
    // composited outputs that have been used in a transaction and should not be spent again.
    // the 'value' is the WalletTransaction index that actually was responsible for locking it.
    std::map<uint64_t, int> m_autoLockedOutputs;
    boost::filesystem::path m_basedir;

    qint64 m_balanceConfirmed = 0;
    qint64 m_balanceImmature = 0;
    qint64 m_balanceUnconfirmed = 0;

    friend class WalletHistoryModel;

    bool m_singleAddressWallet = false;
    bool m_userOwnedWallet = true;

    QList<std::shared_ptr<WalletInfoObject>> m_broadcastingTransactions;

    QList<PaymentRequest*> m_paymentRequests;
};

#endif
