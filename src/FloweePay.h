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
#ifndef FLOWEEPAY_H
#define FLOWEEPAY_H

#include "NotificationManager.h"
#include "WalletEnums.h"

#include <QObject>

#include <DownloadManager.h>
#include <HeaderSyncInterface.h>
#include <WorkerThreads.h>

#include <Mnemonic.h>
#include <QList>
#include <QString>
#include <QDateTime>
#include <uint256.h>


class Wallet;
class NewWalletConfig;
class KeyId;
class PriceDataProvider;
class CameraController;

const std::string &chainPrefix();
QString renderAddress(const KeyId &pubkeyhash);

class FloweePay : public QObject, public WorkerThreads, public HeaderSyncInterface
{
    Q_OBJECT
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString libsVersion READ libsVersion CONSTANT)
    Q_PROPERTY(ApplicationProtection appProtection READ appProtection WRITE setAppProtection NOTIFY appProtectionChanged)
    // p2p net
    Q_PROPERTY(int headerChainHeight READ headerChainHeight NOTIFY headerChainHeightChanged)
    Q_PROPERTY(int expectedChainHeight READ expectedChainHeight NOTIFY expectedChainHeightChanged)
    Q_PROPERTY(int chainHeight READ chainHeight NOTIFY headerChainHeightChanged)
    Q_PROPERTY(bool isMainChain READ isMainChain CONSTANT)
    // GUI user settings
    Q_PROPERTY(int windowWidth READ windowWidth WRITE setWindowWidth NOTIFY windowWidthChanged)
    Q_PROPERTY(int windowHeight READ windowHeight WRITE setWindowHeight NOTIFY windowHeightChanged)
    Q_PROPERTY(bool useDarkSkin READ darkSkin WRITE setDarkSkin NOTIFY darkSkinChanged)
    Q_PROPERTY(bool skinFollowsPlatform READ skinFollowsPlatform WRITE setSkinFollowsPlatform NOTIFY skinFollowsPlatformChanged FINAL)
    Q_PROPERTY(bool hideBalance READ hideBalance WRITE setHideBalance NOTIFY hideBalanceChanged)
    Q_PROPERTY(bool activityShowsBch READ activityShowsBch WRITE setActivityShowsBch NOTIFY activityShowsBchChanged)
    Q_PROPERTY(int fontScaling READ fontScaling WRITE setFontScaling NOTIFY fontScalingChanged)
    Q_PROPERTY(int dspTimeout READ dspTimeout WRITE setDspTimeout NOTIFY dspTimeoutChanged)
    // User setting based on which unit (milli/sats/etc) the user choose
    Q_PROPERTY(int unitAllowedDecimals READ unitAllowedDecimals NOTIFY unitChanged)
    Q_PROPERTY(UnitOfBitcoin unit READ unit WRITE setUnit NOTIFY unitChanged)
    Q_PROPERTY(QString unitName READ unitName NOTIFY unitChanged)
    // notifications
    Q_PROPERTY(bool newBlockMuted READ newBlockMuted WRITE setNewBlockMuted NOTIFY newBlockMutedChanged)
    Q_PROPERTY(bool privateMode READ privateMode WRITE setPrivateMode NOTIFY privateModeChanged)

    // unique helper property..
    Q_PROPERTY(QString paymentProtocolRequest READ paymentProtocolRequest WRITE setPaymentProtocolRequest NOTIFY paymentProtocolRequestChanged FINAL)
    Q_PROPERTY(QString chainPrefix READ qchainPrefix CONSTANT FINAL)
public:
    enum UnitOfBitcoin {
        BCH,
        MilliBCH,
        MicroBCH,
        Bits,
        Satoshis
    };
    Q_ENUM(UnitOfBitcoin)

    /**
     * The protection the user has selected for his Flowee Pay.
     */
    enum ApplicationProtection {
        NoProtection,       //< No application-wide protection. Notice individual wallets can be protected.
        AppPassword,        //< A single password for the app. No encryption.
        AppUnlocked,        //< App-password has been provided.
        WalletsPinToPay,    //< All wallets are PIN-to-Pay
        WalletsPinToOpe     //< All wallets are PIN-to-Open
    };
    Q_ENUM(ApplicationProtection)

    FloweePay();

    /**
     * Select the chain that the FloweePay singleton will be created for.
     * Only has effect if you call this before the first call to instance();
     */
    static void selectChain(P2PNet::Chain chain);
    static FloweePay *instance();
    static void sendTransactionNotification(const P2PNet::Notification &notification);

    QList<Wallet *> wallets() const;

    /**
     * Return the p2pNet owned by this application.
     * Notice that we lazy-initialize this at first call, which is quite expensive.
     * The startP2PInit() method should be called once, to ensure the p2pNet initializes
     * in a worker-thread.
     */
    DownloadManager* p2pNet();

    /**
     * Return the fiat-prices data provider owned by this application.
     */
    PriceDataProvider *prices() const;

    /// return the amount of milli-seconds we wait for a double-spent-proof
    int dspTimeout() const;
    void setDspTimeout(int milliseconds);

    /// return the app data location
    QString basedir() const;

    /**
     * Load p2p layer in a worker-thread.
     * The signal loadComplete() will be triggered when that is done, after which it is safe to call
     * the p2pNet() method.
     * \sa loadComplete();
     */
    void startP2PInit();

    /// for a price, in satoshis, return a formatted string in unitName().
    Q_INVOKABLE inline QString amountToString(double price) const {
        return FloweePay::amountToString(static_cast<qint64>(price), m_unit);
    }
    /// for a price, in satoshis, return a formatted string in unitName(), removing trailing zeros.
    Q_INVOKABLE QString amountToStringPretty(double price) const;
    /// for a price, in satoshis, return a formatted string in unitName().
    QString amountToString(qint64 price) const {
        return FloweePay::amountToString(price, m_unit);
    }
    /// for a price, in satoshis, return a formatted string
    static QString amountToString(qint64 price, UnitOfBitcoin unit);

    Q_INVOKABLE QString formatDate(QDateTime date) const;
    Q_INVOKABLE QString formatDateTime(QDateTime datetime) const;
    Q_INVOKABLE QString formatBlockTime(int blockHeight) const;

    /// create a new HD wallet with an optional name.
    Q_INVOKABLE NewWalletConfig* createNewWallet(const QString &derivationPath, const QString &password = QString(), const QString &walletName = QString());
    /// create a new seed-less wallet with an optional name.
    Q_INVOKABLE NewWalletConfig* createNewBasicWallet(const QString &walletName = QString());
    /// swipe a paper wallet with an optional name
    Q_INVOKABLE NewWalletConfig* createImportedWallet(const QString &privateKey, const QString &walletName, int startHeight);
    /// Alternative arguments version. With date instead of block-height
    Q_INVOKABLE NewWalletConfig* createImportedWallet(const QString &privateKey, const QString &walletName, const QDateTime &date);

    /// Find out about the address and return an instance of AddressInfo if known.
    Q_INVOKABLE QObject* researchAddress(const QString &address, QObject *parent);

    /**
     * If property appProtection is set to AppPassword, this method will return true
     * only if the provided password is the one set by the user.
     */
    Q_INVOKABLE bool checkAppPassword(const QString &password);
    Q_INVOKABLE void setAppPassword(const QString &password);

    /**
     * Import a mnemonics based (BIP39)  wallet.
     * Warning; will throw if the mnemonic is invalid
     *
     * @param mnemonic, the list of words (validated!) in the BIP39 format.
     * @param password, the optional password that goes with a BIP39 wallet.
     * @param derivationPath the path that turns this mnemonic into a hierarchically deterministic wallet.
     * @param walletName the name the user knows this wallet as.
     * @param startHeight the first block we should check for transactions, or zero for "future blocks"
     *        If you set this to 1 then we set it to a more sane value of when Bitcoin became more well known.
     * @param electrumFormat if true, interpret the mnemonic phrase using the Electrum format rather than BIP39.
     */
    Q_INVOKABLE NewWalletConfig *createImportedHDWallet(const QString &mnemonic, const QString &password,
                                                        const QString &derivationPath, const QString &walletName, int startHeight = 0,
                                                        bool electrumFormat = false);
    /// Alternative arguments version. With date instead of block-height
    Q_INVOKABLE NewWalletConfig *createImportedHDWallet(const QString &mnemonic, const QString &password,
                                                        const QString &derivationPath, const QString &walletName, const QDateTime &date,
                                                        bool electrumFormat = false);

    Q_INVOKABLE bool checkDerivation(const QString &path) const;

    /// take a bitcoin-address and identify the type.
    Q_INVOKABLE WalletEnums::StringType identifyString(const QString &string) const;

    /// return a string version of the \a unit name. tBCH for instance.
    Q_INVOKABLE QString nameOfUnit(FloweePay::UnitOfBitcoin unit) const;

    /**
     * Change the currency based on the country code (nl_NL / en_US)
     */
    Q_INVOKABLE void setCountry(const QString &countrycode);
    Q_INVOKABLE QStringList recentCountries() const;

    /// returns the unit of our prices. BCH, for instance.
    QString unitName() const;
    /**
     * Returns the amount of digits allowed behind the decimal-separator.
     * The 'unit' the user pics (BCH/milli-bch/micro-bch) just moves the decimal separator
     * and we measure that by the amount of digits we still allow behind the separator.
     * For the "BCH" unit this is 8, for sats this is zero.
     */
    int unitAllowedDecimals() const;

    int windowWidth() const;
    void setWindowWidth(int windowWidth);

    int windowHeight() const;
    void setWindowHeight(int windowHeight);

    bool darkSkin() const;
    void setDarkSkin(bool darkSkin);

    bool skinFollowsPlatform() const;
    void setSkinFollowsPlatform(bool newSkinFollowsPlatform);

    UnitOfBitcoin unit() const;
    void setUnit(const UnitOfBitcoin &unit);

    /**
     * Return the chain-height of validated headers.
     * When we connect to some peers we get updated headers and thus the first
     * indication of the world status is the header chain height.
     */
    int headerChainHeight() const;

    /**
     * Return the chain-height that based on the date/time we expect
     * to be at.
     */
    int expectedChainHeight() const;

    /**
     * The best known chainHeight.
     * On startup the headerChainHeight is likely going to be outdated
     * as much as our accountBlockHeight is. Which would result in bad info.
     * For the time we have not yet gotten the headers therefore we will
     * use the mathematically determined expectedChainHeight. We stop using
     * that as soon as we get some movement in the headers one as the expected
     * one may be off by several blocks.
     */
    int chainHeight();

    // HeaderSyncInterface interface
    void setHeaderSyncHeight(int height) override;
    void headerSyncComplete() override;

    /**
     *  Returns true if this is the mainchain.
     *  \see selectChain()
     */
    bool isMainChain() const {
        return m_chain == P2PNet::MainChain;
    }
    /**
     *  Returns the chain this Pay was created with.
     *  \see selectChain()
     */
    P2PNet::Chain chain() const;
    const std::string &chainPrefix() const { return m_chainPrefix; }
    QString qchainPrefix() const; // for the QML property

    /**
     * return if the user engaged the hide-balance feature to avoid people seeing the balance on-screen.
     */
    bool hideBalance() const;
    /**
     * Set the hide-balance on or off.
     */
    void setHideBalance(bool hideBalance);

    Q_INVOKABLE void copyToClipboard(const QString &text);
    Q_INVOKABLE void openInExplorer(const QString &txid);

    QString version() const;
    QString libsVersion() const;

    /// Simple bool to register user preference
    bool isOffline() const;
    /// register the user preference for being offline.
    void setOffline(bool offline);

    /**
     * run the p2p networking agents, unless isOffline() is true.
     * After the startP2PInit has successfully completed, call this
     * to start the actions running on the network layer which will
     * sync the databases from the p2p net.
     */
    void startNet();

    /// If true, no notifications about new blocks will be shown
    bool newBlockMuted() const;
    /// If true, no notifications about new blocks will be shown
    void setNewBlockMuted(bool mute);

    // return the CameraController previously set on this app singleton
    CameraController *cameraController();
    // set the cameraController for the singleton to take ownership of.
    void setCameraController(CameraController *cc);

    int fontScaling() const;
    void setFontScaling(int newFontScaling);

    bool activityShowsBch() const;
    void setActivityShowsBch(bool newActivityShowsBch);

    bool privateMode() const;
    void setPrivateMode(bool newPrivateMode);

    ApplicationProtection appProtection() const;
    void setAppProtection(ApplicationProtection newAppProtection);

    QString paymentProtocolRequest() const;
    void setPaymentProtocolRequest(const QString &newPaymentProtocolRequest);

signals:
    void loadComplete();
    /// \internal
    void loadComplete_priv();
    /// \internal
    void startSaveData_priv();
    void unitChanged();
    void walletsChanged();
    void darkSkinChanged();
    void windowWidthChanged();
    void windowHeightChanged();
    void headerChainHeightChanged();
    void expectedChainHeightChanged();
    void dspTimeoutChanged();
    void hideBalanceChanged();
    void newBlockMutedChanged();
    void fontScalingChanged();
    void activityShowsBchChanged();
    void totalBalanceConfigChanged();
    void privateModeChanged();
    void appProtectionChanged();
    void paymentProtocolRequestChanged();

    void skinFollowsPlatformChanged();

private slots:
    void loadingCompleted();
    void saveData();

private:
    struct AccountConfigData {
        // a per-wallet bool indicating its balance should be counted in the whole
        bool countBalance = true;
        bool privateWallet = false; // is hidden in private mode
        bool allowInstaPay = false;
        // per currency-code upper limit where payments are auto-approved.
        QMap<QString, int> fiatInstaPayLimits;
    };

    void init();
    void shutdown();
    void saveAll();
    // create wallet and add to list. Please consider calling walletsChanged() after
    Wallet *createWallet(const QString &name);
    uint32_t walletStartHeightHint() const;
    void connectToWallet(Wallet *wallet);

    mutable Mnemonic m_hdSeedValidator;

    UnitOfBitcoin m_unit = BCH;
    ApplicationProtection m_appProtection = NoProtection;
    P2PNet::Chain m_chain = P2PNet::MainChain;
    uint256 m_appProtectionHash;
    QString m_basedir;
    std::string m_chainPrefix;
    std::unique_ptr<DownloadManager> m_downloadManager;
    std::unique_ptr<PriceDataProvider> m_prices;
    NotificationManager m_notifications;
    CameraController* m_cameraController;
    QList<Wallet*> m_wallets;
    QHash<uint16_t, AccountConfigData> m_accountConfigs; // key is wallet-segment-id
    int m_dspTimeout = 5000;
    int m_windowWidth = 500;
    int m_windowHeight = 500;
    int m_fontScaling = 100;
    bool m_loadingCompleted = false; // 'init()' completed
    bool m_loadCompletEmitted = false; // ensure we only emit this once.
    bool m_appUnlocked = false;
    bool m_darkSkin = true;
    bool m_skinFollowsPlatform = false;
    bool m_createStartWallet = false;
    bool m_hideBalance = false;
    /// Show the BCH amount involved in a transaction on the activity screen.
    bool m_activityShowsBch = false;
    bool m_offline = false;
    bool m_gotHeadersSyncedOnce = false;
    bool m_privateMode = false; // wallets marked private are hidden when true

    // the string passed in the app in order to instantly start paying.
    QString m_paymentProtocolRequest;

#ifdef TARGET_OS_Android
    // when the app is no longer the front app we record the time in order to
    // know how much time has passed when we get active again.
    QDateTime m_sleepStart;
#endif
    friend class AccountConfig;
};

#endif
