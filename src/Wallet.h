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
#ifndef FLOWEE_WALLET_H
#define FLOWEE_WALLET_H

#include <QObject>
#include <QMutex>

#include <DataListenerInterface.h>
#include <HeaderSyncInterface.h>
#include <PrivacySegment.h>

#include <HDMasterKey.h>
#include <HDMasterPubkey.h>
#include <primitives/key.h>
#include <primitives/pubkey.h>
#include <primitives/Tx.h>

#include <boost/unordered_map.hpp>
#include <boost/filesystem.hpp>

class WalletInfoObject;
class TransactionInfo;
struct WalletInsertBeforeData; // private wallet data struct.

namespace P2PNet {
struct Notification;
}

class Wallet : public QObject, public DataListenerInterface, public HeaderSyncInterface
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
    Wallet(const boost::filesystem::path &basedir, uint16_t segmentId, uint32_t encryptionSeed = 0);
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

        inline bool isValid() const {
            return m_txid != 0;
        }

        Wallet::OutputRef& operator=(const OutputRef&) = default;

    private:
        uint32_t m_txid = 0; // index in m_walletTransactions
        uint16_t m_outputIndex = 0;
    };

    enum SignatureType {
        SignedAsEcdsa = 0,
        SignedAsSchnorr,
        NotUsedYet,
    };

    /**
     * @brief newTransactions announces a list of transactions pushed to us from a peer.
     * @param header the block header these transactions appeared in.
     * @param blockHeight the blockheight we know the header under.
     * @param blockTransactions The actual transactions.
     */
    void newTransactions(const uint256 &blockId, int blockHeight, const std::deque<Tx> &blockTransactions) override;
    // notify about unconfirmed Tx.
    void newTransaction(const Tx &tx) override;
    void rebuildFilter() override {
        rebuildBloom();
    }
    /// Let the wallet know that it is up-to-date to \a height
    void setLastSynchedBlockHeight(int height) override;
    void headerSyncComplete() override;

    PrivacySegment *segment() const;

    /// Create a new private key.
    void createNewPrivateKey(uint32_t currentBlockheight);
    /// import an existing private key.
    bool addPrivateKey(const QString &privKey, uint32_t startBlockHeight);
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
    Tx::Output txOutput(OutputRef ref) const;
    /// Fetch UTXO value (in sats)
    qint64 utxoOutputValue(OutputRef ref) const;

    struct PrivKeyData {
        int privKeyId = 0;
        PrivateKey key;
        SignatureType sigType;
    };
    /// Fetch UTXO key
    PrivKeyData unlockKey(OutputRef ref) const;

    /**
     * Register the signature type for the given private key.
     * The unlockKey() method will return the sigType stored for
     * a specific private key. By calling this method we
     * store the signature type for this key, for future requests.
     */
    void updateSignatureType(const PrivKeyData &data);

    /// Return a bitcoin address (160 bits ripe key) for deposit.
    KeyId nextUnusedAddress();

    /// Return a bitcoin address (160 bits ripe key) for change.
    KeyId nextUnusedChangeAddress();

    enum PrivKeyType {
        ChangePath,     ///< The private keys created from a HD wallets change derivation
        ReceivePath     ///< All the other types of private keys
    };

    /// Return a private-key-index for deposits and reserve it from re-use.
    int reserveUnusedAddress(KeyId &keyId, PrivKeyType pkt = ReceivePath);

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

    /// a simple boolean that indicates only address zero will ever be used.
    bool isSingleAddressWallet() const;
    void setSingleAddressWallet(bool on);

    // Fill the transactionInfo object from walletTransaction \a txIndex
    void fetchTransactionInfo(TransactionInfo *info, int txIndex);

    /**
     * Returns a 'seed' to add to the user password on a (partially) encrypted
     * wallet.
     * \see encryptWallet()
     */
    uint32_t encryptionSeed() const;

    /**
     * For an already encrypted wallet, set the seed.
     * The usage is, in order;
     * @code
     *   setEncryptionSeed(walletSeed);
     *   setEncryptionLevel(walletLevel, userPassword);
     * @code
     *
     * After this methods like decrypt() become available as well as the ability
     * to sync a wallet that auto-generates its own keys.
     */
    void setEncryptionSeed(uint32_t newEncryptionSeed);

    enum EncryptionLevel {
        NotEncrypted,
        /**
         * This will encrypt the private keys of the wallet, which effectively
         * removes only one functio of the wallet, to sign transactions spending funds.
         */
        SecretsEncrypted,
        /**
         * This level of encryption aims to encrypt all functionality of
         * the wallet to the level that even reading the on-disk files
         * an attacker will not be able to figure out which addresses are ours.
         */
        FullyEncrypted
    };

    /**
     * Set the level of encryption applied to this wallet's saved state.
     *
     * When the wallet changes from NotEncrypted to a higher level, we
     * calculate an encryptionSeed() and (re)write an encrypted version
     * of the wallet to disk.
     *
     * Setting a level higher than NotEncrypted will remove the relevant
     * data from the in-memory representation of this wallet, requiring
     * a call to decrypt() before usage.
     */
    void setEncryption(EncryptionLevel level, const QString &password);
    EncryptionLevel encryption() const;
    /// Read encrypted secrets from disk into memory
    bool decrypt(const QString &password);
    bool isDecrypted() const;

    // forget all encrypted secrets.
    void forgetEncryptedSecrets();

#ifdef IN_TESTS
    /**
     * Unit tests can call this to populate a wallet with some dummy transactions.
     */
    void addTestTransactions();
#endif

    /**
     * When true, then this wallet is the result of a user-initiated action.
     * Otherwise it was created by the system for some reason and is empty.
     */
    bool userOwnedWallet() const;
    /**
     * Changes a wallet to be user-owned or not.
     */
    void setUserOwnedWallet(bool userOwnedWallet);

    /**
     * Set comment field on this transaction, assuming it has been accepted by this wallet.
     */
    void setTransactionComment(const Tx &transaction, const QString &comment);

    struct WalletSecret {
        PrivateKey privKey;
        std::vector<char> encryptedPrivKey;
        KeyId address;
        /*
         * initial height for a secret key is relevant for the getmerkleblock call.
         * The special value 0 implies that the key is created but not yet shared and thus
         * initial height is still in the future.
         */
        uint32_t initialHeight = 0;
        SignatureType signatureType = NotUsedYet;
        bool fromHdWallet = false;
        bool fromChangeChain = false;
        int hdDerivationIndex = -1;
        /// if true, this address has been reseved to receive funds on
        bool reserved = false; // in-mem-only

        void setPrivKey(const Streaming::ConstBuffer &data);
    };

    /// Return the private keys and details owned by this wallet.
    std::map<int, WalletSecret> walletSecrets() const;

    struct KeyDetails {
        qint64 saldo;          ///< The amount of Satoshi currently on this key.
        int coins;              ///< The amount of coins that make up the current saldo
        int historicalCoins;    ///< The amount of coins ever seen on this key.
    };
    KeyDetails fetchKeyDetails(int privKeyId) const;
    int findPrivKeyId(const KeyId &address) const;

    /// Returns true if this wallet is backed by a Hierarchically Deterministic seed.
    bool isHDWallet() const;
    /// Provided that this is a HD wallet, return the seed-words (aka mnemonic)
    QString hdWalletMnemonic() const;
    /// Provided that this is a HD wallet, return the seed-words (aka mnemonic) passphrase
    QString hdWalletMnemonicPwd() const;
    /// Provided that this is a HD wallet, return the derivation path used for this wallet.
    QString derivationPath() const;
    /// Provided that this is a HD wallet, return the xpub for this wallet.
    QString xpub() const;


    /**
     * Create a HD masterkey that will be used for future creation of new private keys
     *
     * @param mnemonic The seed-string. Please validate the mnemonic before you pass it in here! This method assumes it is valid.
     * @param pwd the password that was created with the seed-phrase. Can be empty.
     * @param derivationPath the derivation steps. We will add 2 ints to this internally, so typically this vector has 3 fields.
     * @param initialBlockHeight either a timestamp or blockheight, or zero if unsure.
     */
    void createHDMasterKey(const QString &mnemonic, const QString &pwd, const std::vector<uint32_t> &derivationPath, uint32_t startHeight = 0);

    /// return the height of the last seen transaction that is mined
    int lastTransactionTimestamp() const;

    /**
     * Lock a the output from being spent in auto-generated transactions.
     * @returns true if successful, false if the UTXO did not exist.
     */
    bool lockUTXO(OutputRef outputRef);
    bool unlockUTXO(OutputRef outputRef);
    bool isLocked(OutputRef outputRef) const;

    /// Return the blockheight at which this wallet was created
    int walletCreatedHeight() const;

    /// Check the loaded wallet version Id and make internal changes to upgrade it to current.
    void performUpgrades();

    bool walletIsImporting() const;

public slots:
    void delayedSave();

protected slots:
    void broadcastTxFinished(int txIndex, bool success);
    /// find all not-yet-confirmed transactions and start a broadcast
    void broadcastUnconfirmed();
    void recalculateBalance();
    void delayedSaveTimeout();

signals:
    void balanceChanged();
    void utxosChanged();
    void appendedTransactions(int firstNew, int count);
    void lastBlockSynchedChanged();
    void userOwnedChanged();
    void transactionChanged(int txIndex);
    void transactionConfirmed(int txIndex);
    void transactionRemoved(int txIndex);
    void encryptionChanged();

    // \internal
    void startDelayedSave();

protected:
    Wallet();

#ifndef IN_TESTS
private:
#endif
    /// see also saveWallet()
    void loadWallet();
    // called by loadSecrets and decrypt()
    Streaming::ConstBuffer readSecrets() const;
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

    // returns true if any of the transactions are unknown to the wallet
    bool anythingNew(int blockHeight, const std::deque<Tx> &transactions) const;

    // helper method called from both newTransaction and newTransactions
    void updateSignatureTypes(const std::map<uint64_t, SignatureType> &txData);

    /// Helper method for findInputsFor
    int scoreForSolution(const OutputSet &set, int64_t change,
                         size_t unspentOutputCount) const;

    std::unique_ptr<PrivacySegment> m_segment;
    mutable QRecursiveMutex m_lock;
    /// used to determine if we need to persist the wallet
    bool m_walletChanged = false;
    /// Used to determine if we need to save the wallet-name file
    bool m_walletNameChanged = false;
    /// used to determine if we need to persist the private keys
    bool m_secretsChanged = false;

    int m_nextWalletSecretId = 1;
    int m_nextWalletTransactionId = 1;
    int m_lastBlockHeightSeen = 0;
    short m_bloomScore = 0;
    std::map<int, WalletSecret> m_walletSecrets;

    struct HierarchicallyDeterministicWalletData {
        /// the strings should have utf8 encoded text.
        HierarchicallyDeterministicWalletData(const Streaming::ConstBuffer &seedWords, const std::vector<uint32_t> &derivationPath, const Streaming::ConstBuffer &pwd);
        HierarchicallyDeterministicWalletData(const std::string &xpub, const std::vector<uint32_t> &derivationPath);
        HDMasterKey masterKey;
        HDMasterPubkey masterPubkey;
        std::vector<char, secure_allocator<char> > walletMnemonic; // utf8-encoding
        std::vector<char, secure_allocator<char> >  walletMnemonicPwd;// utf8-encoding

        std::vector<char> encryptedWalletMnemonic;
        std::vector<char> encryptedWalletMnemonicPwd;

        std::vector<uint32_t> derivationPath; // contains the last created privkey. (full derivation path)
        int lastMainKey = -1;   // for derivation {BASE} + 0 / [num]
        int lastChangeKey = -1; // for derivation {BASE} + 1 / [num]
        // two mem-only limits indicating the last key included in the bloom filter.
        int lastIncludedMainKey = -1;
        int lastIncludedChangeKey = -1;
    };
    std::unique_ptr<HierarchicallyDeterministicWalletData> m_hdData;

    QString m_name;
    int m_walletVersion;

    struct Output {
        int walletSecretId = -1;
        uint64_t value = 0;
    };

    struct WalletTransaction {
        uint256 txid;
        uint256 minedBlock;
        int minedBlockHeight = 0;
        bool isCoinbase = false;
        bool isCashFusionTx = false;
        QString userComment;

        bool isUnconfirmed() const;
        bool isRejected() const;

        /*
         * To keep track of chained transactions we store for an input a entry here,
         * should it come from an output stored in the walletTransactins map.
         *
         * Notice that only outputs matching one of our keys are eligable, so these
         * are inputs that spent them.
         *
         * Key: index of input.
         * Value: encoded OutputRef
         */
        std::map<int, uint64_t> inputToWTX;
        std::map<int, Output> outputs; // output-index to Output (only ones we can/could spend)
    };

    /**
     * Parse a transaction and match the inputs / outputs to our UTXO and known addresses.
     * This creates a fully populated WalletTransaction instance.
     *
     * @param tx the original, input transaction.
     * @param txid the transaction-id for tx
     * @param types[out] this map is filled with inputs and the signature types used.
     * @param change an optional notification instance to highlight the changes that this transaction causes.
     */
    WalletTransaction createWalletTransactionFromTx(const Tx &tx, const uint256 &txid, std::map<uint64_t, SignatureType> &types, P2PNet::Notification *change = nullptr) const;
    void saveTransaction(const Tx &tx);
    /**
     * returns a Tx if this txid was saved in this wallet.
     *
     * pool.reserve() is called by this method with the actual tx size.
     */
    Tx loadTransaction(const uint256 &txid, Streaming::BufferPool &pool) const;

    struct InsertBeforeData
    {
        InsertBeforeData(Wallet *wallet);
        // on destructor we re-insert the transactions.
        ~InsertBeforeData();
        std::vector<WalletTransaction> transactions;
        std::set<int> oldTransactionIds;
        Wallet *parent;
    };
    bool m_inInsertBeforeCallback = false;

    /*
     * Our UTXO based system requires interpretation of state based on explicit ordering.
     * So if we get something inserted out-of-order we're going to rollback the ones we already
     * have AFTER the header and re-apply them in the destructor of the InsertBeforeData struct.
     */
    InsertBeforeData removeTransactionsAfter(int blockHeight);

    /// helper method to upgrade older wallets and find the sigType from transactions to populate the field in walletSecrets.
    /// Returns true if any changes were made.
    void populateSigType();

    // check if we need to create more private keys based on if this transaction
    // used private keys close to the index we have created and keep track off.
    // returns true if new private keys have been derived from the HD masterkey
    bool updateHDSignatures(const WalletTransaction &wtx, bool &updateBloom);

    /// use the hdData master key to create a number of private keys (WalletSecrets).
    void deriveHDKeys(int mainChain, int changeChain, uint32_t startHeight = 0);

    std::map<int, WalletTransaction> m_walletTransactions;

    typedef boost::unordered_map<uint256, int, HashShortener> TxIdCash;
    TxIdCash m_txidCache; // txid -> m_walletTransactions-id

    boost::filesystem::path m_basedir;
    /*
     * Our little UTXO.
     * OutputRef -> value (in sat)
     */
    std::map<uint64_t, uint64_t> m_unspentOutputs;
    /*
     * Unspent outputs can be 'locked' from spending for several reasons. User decided or
     * simply because an unconfirmed tx spent it.
     * We remember those here, where the 'key-value' pair follows the same concept for the
     * key as the m_unspentOutputs and
     * the 'value' is the WalletTransaction index that actually was responsible for locking it.
     * or it is 0 if the locking was user-decided.
     *
     * The '-1' value is used to indicate this is a dusting attack UTXO.
     * Other negative numbers are used to indicate the UTXO is a coinbase and the value is the
     * height at which it matures, times -1. (example: -101 for block at height 1)
     */
    std::map<uint64_t, int> m_lockedOutputs;

    qint64 m_balanceConfirmed = 0;
    qint64 m_balanceImmature = 0;
    qint64 m_balanceUnconfirmed = 0;

    friend class WalletHistoryModel;
    friend class WalletSecretsModel;
    friend class WalletCoinsModel;

    // auto-detected, causes us to send bigger gaps for bloom filters.
    bool m_walletStoresCashFusions = false;
    bool m_walletIsImporting = false; // only true for the initial wallet import.
    // user settings
    bool m_singleAddressWallet = false;
    bool m_userOwnedWallet = true;

    // operational
    /// Remove the encryption key (created from password), making the wallet unable to save private keys.
    void clearEncryptionKey();
    /// turn the password into a key. Return false if it is not the correct one.
    bool parsePassword(const QString &password);
    bool m_saveStarted = false;
    bool m_haveEncryptionKey = false;
    uint32_t m_encryptionSeed = 0;
    uint16_t m_encryptionChecksum = 0;
    EncryptionLevel m_encryptionLevel = NotEncrypted;
    std::vector<char, secure_allocator<char>> m_encryptionKey;
    std::vector<char, secure_allocator<char>> m_encryptionIR;

    QList<std::shared_ptr<WalletInfoObject>> m_broadcastingTransactions;
};

#endif
