/*
 * This file is part of the Flowee project
 * Copyright (C) 2020-2022 Tom Zander <tom@flowee.org>
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
#include "PriceHistoryDataProvider.h"
#include "WalletEnums.h"

#include <QObject>

#include <DownloadManager.h>
#include <P2PNetInterface.h>
#include <WorkerThreads.h>

#include <Mnemonic.h>
#include <QList>
#include <QString>
#include <QDateTime>


namespace Streaming {
    class BufferPool;
}
class Wallet;
class NewWalletConfig;
class KeyId;
class PriceDataProvider;
class PriceHistoryDataProvider;

const std::string &chainPrefix();
QString renderAddress(const KeyId &pubkeyhash);

class FloweePay : public QObject, WorkerThreads, P2PNetInterface
{
    Q_OBJECT
    Q_PROPERTY(QString unitName READ unitName NOTIFY unitChanged)
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString libsVersion READ libsVersion CONSTANT)
    Q_PROPERTY(int windowWidth READ windowWidth WRITE setWindowWidth NOTIFY windowWidthChanged)
    Q_PROPERTY(int windowHeight READ windowHeight WRITE setWindowHeight NOTIFY windowHeightChanged)
    Q_PROPERTY(int unitAllowedDecimals READ unitAllowedDecimals NOTIFY unitChanged)
    Q_PROPERTY(int headerChainHeight READ headerChainHeight NOTIFY headerChainHeightChanged)
    Q_PROPERTY(int expectedChainHeight READ expectedChainHeight NOTIFY expectedChainHeightChanged)
    Q_PROPERTY(int chainHeight READ chainHeight NOTIFY headerChainHeightChanged)
    Q_PROPERTY(bool useDarkSkin READ darkSkin WRITE setDarkSkin NOTIFY darkSkinChanged)
    Q_PROPERTY(bool isMainChain READ isMainChain CONSTANT)
    Q_PROPERTY(bool hideBalance READ hideBalance WRITE setHideBalance NOTIFY hideBalanceChanged)
    Q_PROPERTY(bool newBlockMuted READ newBlockMuted WRITE setNewBlockMuted NOTIFY newBlockMutedChanged);
    Q_PROPERTY(UnitOfBitcoin unit READ unit WRITE setUnit NOTIFY unitChanged)
    Q_PROPERTY(int dspTimeout READ dspTimeout WRITE setDspTimeout NOTIFY dspTimeoutChanged)
    Q_PROPERTY(int fontScaling READ fontScaling WRITE setFontScaling NOTIFY fontScalingChanged)
    Q_PROPERTY(QString platform READ platform CONSTANT)
public:
    enum UnitOfBitcoin {
        BCH,
        MilliBCH,
        MicroBCH,
        Bits,
        Satoshis
    };

    FloweePay();
    ~FloweePay();

    /**
     * Select the chain that the FloweePay singleton will be created for.
     * Only has effect if you call this before the first call to instance();
     */
    static void selectChain(P2PNet::Chain chain);
    static FloweePay *instance();

    QList<Wallet *> wallets() const;

    DownloadManager* p2pNet();

    PriceDataProvider *prices() const;
    PriceHistoryDataProvider *priceHistory() const;

    /// return the amount of milli-seconds we wait for a double-spent-proof
    int dspTimeout() const;
    void setDspTimeout(int milliseconds);

    /// return the app data location
    QString basedir() const;

    /// returns platform name, Linux / Android / etc
    QString platform() const;

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
     * Import a mnemonics based (BIP39)  wallet.
     * Warning; will throw if the mnemonic is invalid
     *
     * @param mnemonic, the list of words (validated!) in the BIP39 format.
     * @param password, the optional password that goes with a BIP39 wallet.
     * @param derivationPath the path that turns this mnemonic into a hierarchically deterministic wallet.
     * @param walletName the name the user knows this wallet as.
     * @param startHeight the first block we should check for transactions, or zero for "future blocks"
     *        If you set this to 1 then we set it to a more sane value of when Bitcoin became more well known.
     */
    Q_INVOKABLE NewWalletConfig *createImportedHDWallet(const QString &mnemonic, const QString &password,
                                                        const QString &derivationPath, const QString &walletName, int startHeight = 0);
    /// Alternative arguments version. With date instead of block-height
    Q_INVOKABLE NewWalletConfig *createImportedHDWallet(const QString &mnemonic, const QString &password,
                                                        const QString &derivationPath, const QString &walletName, const QDateTime &date);

    Q_INVOKABLE bool checkDerivation(const QString &path) const;

    /// take a bitcoin-address and identify the type.
    Q_INVOKABLE WalletEnums::StringType identifyString(const QString &string) const;

    /// return a string version of the \a unit name. tBCH for instance.
    Q_INVOKABLE QString nameOfUnit(FloweePay::UnitOfBitcoin unit) const;

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

    // P2PNetInterface interface
    void blockchainHeightChanged(int newHeight);

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

    /**
     * return if the user engaged the hide-balance feature to avoid people seeing the balance on-screen.
     */
    bool hideBalance() const;
    /**
     * Set the hide-balance on or off.
     */
    void setHideBalance(bool hideBalance);

    Q_INVOKABLE void copyToClipboard(const QString &text);
    Q_INVOKABLE void openInExplorer(const QString &text);

    QString version() const;
    QString libsVersion() const;

    /// Simple bool to register user preference
    bool isOffline() const;
    /// register the user preference for being offline.
    void setOffline(bool offline);

    /// start the p2p networking, unless isOffline()
    void startNet();

    /// If true, no notifications about new blocks will be shown
    bool newBlockMuted() const;
    /// If true, no notifications about new blocks will be shown
    void setNewBlockMuted(bool mute);


    int fontScaling() const;
    void setFontScaling(int newFontScaling);

signals:
    void loadComplete();
    /// \internal
    void loadComplete_priv();
    /// \internal
    void startSaveDate_priv();
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

private slots:
    void loadingCompleted();
    void saveData();

private:
    void init();
    void saveAll();
    // create wallet and add to list. Please consider calling saveData after
    Wallet *createWallet(const QString &name);
    uint32_t walletStartHeightHint() const;

    mutable Mnemonic m_hdSeedValidator;

    UnitOfBitcoin m_unit = BCH;
    QString m_basedir;
    P2PNet::Chain m_chain = P2PNet::MainChain;
    std::string m_chainPrefix;
    std::unique_ptr<DownloadManager> m_downloadManager;
    std::unique_ptr<PriceDataProvider> m_prices;
    std::unique_ptr<PriceHistoryDataProvider> m_priceHistory;
    NotificationManager m_notifications;
    QList<Wallet*> m_wallets;
    int m_dspTimeout = 5000;
    int m_windowWidth = 500;
    int m_windowHeight = 500;
    int m_initialHeaderChainHeight = 0;
    int m_fontScaling = 100;
    bool m_darkSkin = true;
    bool m_createStartWallet = false;
    bool m_hideBalance = false;
    bool m_offline = false;
};

#endif
