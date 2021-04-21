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
#ifndef FLOWEEPAY_H
#define FLOWEEPAY_H

#include "NotificationManager.h"

#include <QObject>

#include <DownloadManager.h>
#include <P2PNetInterface.h>
#include <WorkerThreads.h>

#include <QList>
#include <QString>

const std::string &chainPrefix();

namespace Streaming {
    class BufferPool;
}
class Wallet;

class FloweePay : public QObject, WorkerThreads, P2PNetInterface
{
    Q_OBJECT
    Q_PROPERTY(QString unitName READ unitName NOTIFY unitChanged)
    Q_PROPERTY(int windowWidth READ windowWidth WRITE setWindowWidth NOTIFY windowWidthChanged)
    Q_PROPERTY(int windowHeight READ windowHeight WRITE setWindowHeight NOTIFY windowHeightChanged)
    Q_PROPERTY(int unitAllowedDecimals READ unitAllowedDecimals NOTIFY unitChanged)
    Q_PROPERTY(int headerChainHeight READ headerChainHeight NOTIFY headerChainHeightChanged)
    Q_PROPERTY(int expectedChainHeight READ expectedChainHeight NOTIFY expectedChainHeightChanged)
    Q_PROPERTY(int chainHeight READ chainHeight NOTIFY headerChainHeightChanged)
    Q_PROPERTY(bool useDarkSkin READ darkSkin WRITE setDarkSkin NOTIFY darkSkinChanged)
    Q_PROPERTY(bool isMainChain READ isMainChain CONSTANT)
    Q_PROPERTY(UnitOfBitcoin unit READ unit WRITE setUnit NOTIFY unitChanged)
    Q_PROPERTY(int dspTimeout READ dspTimeout WRITE setDspTimeout NOTIFY dspTimeoutChanged)
public:
    enum StringType {
        Unknown = 0,
        PrivateKey,
        CashPKH,
        CashSH,
        LegacyPKH,
        LegacySH
    };
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

    /// return a pool for the current thread;
    static Streaming::BufferPool &pool(int reserveSize);

    /// return the amount of milli-seconds we wait for a double-spent-proof
    int dspTimeout() const;
    void setDspTimeout(int milliseconds);

    /// return the app data location
    QString basedir() const;

    /// for a price, in satoshis, return a formatted string in unitName().
    Q_INVOKABLE inline QString priceToString(double price) const {
        return FloweePay::priceToString(static_cast<qint64>(price), m_unit);
    }
    /// for a price, in satoshis, return a formatted string in unitName().
    QString priceToString(qint64 price) const {
        return FloweePay::priceToString(price, m_unit);
    }
    /// for a price, in satoshis, return a formatted string
    static QString priceToString(qint64 price, UnitOfBitcoin unit);

    /// create a new wallet with an optional name.
    Q_INVOKABLE void createNewWallet(const QString &walletName = QString());
    /// swipe a paper wallet with an optional name
    Q_INVOKABLE void createImportedWallet(const QString &privateKey, const QString &walletName);

    /// take a bitcoin-address and identify the type.
    Q_INVOKABLE FloweePay::StringType identifyString(const QString &string) const;

    /// returns the unit of our prices. BCH, for instance.
    QString unitName() const;
    /// returns the amount of digits allowed behind the decimal-separator.
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

    Q_INVOKABLE void copyToClipboard(const QString &text);

    Q_ENUM(StringType UnitOfBitcoin)
signals:
    void loadComplete();
    /// \internal
    void loadComplete_priv();

    void unitChanged();
    void walletsChanged();
    void darkSkinChanged();

    void windowWidthChanged();
    void windowHeightChanged();
    void headerChainHeightChanged();
    void expectedChainHeightChanged();
    void dspTimeoutChanged();

private:
    void init();
    void saveData();
    void saveAll();
    Wallet *createWallet(const QString &name);
    int walletStartHeightHint() const;


    UnitOfBitcoin m_unit = BCH;
    QString m_basedir;
    P2PNet::Chain m_chain = P2PNet::MainChain;
    std::string m_chainPrefix;
    std::unique_ptr<DownloadManager> m_downloadManager;
    NotificationManager m_notifications;
    QList<Wallet*> m_wallets;
    int m_dspTimeout;
    int m_windowWidth;
    int m_windowHeight;
    int m_initialHeaderChainHeight = 0;
    bool m_darkSkin;
    bool m_createStartWallet = false;
};

#endif
