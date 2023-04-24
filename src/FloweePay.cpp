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
#include "FloweePay.h"
#include "Wallet.h"
#include "NewWalletConfig.h"
#include "AddressInfo.h"
#include "PriceDataProvider.h"

#include <base58.h>
#include <ripemd160.h>
#include <cashaddr.h>
#include <streaming/MessageParser.h>
#include <streaming/BufferPools.h>
#include <streaming/MessageBuilder.h>
#include <random.h>
#include <config/flowee-config.h>
#include <SyncSPVAction.h>

#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QLocale>
#include <QResource>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>

constexpr const char *UNIT_TYPE = "unit";
constexpr const char *CREATE_START_WALLET = "create-start-wallet";
constexpr const char *WINDOW_WIDTH = "window/width";
constexpr const char *WINDOW_HEIGHT = "window/height";
constexpr const char *FONTSCALING = "window/font-scaling";
constexpr const char *DARKSKIN = "darkSkin";
constexpr const char *ACTIVITYSHOWBCH = "activity-show-bch";
constexpr const char *HIDEBALANCE = "hide-balance";
constexpr const char *USERAGENT = "net/useragent";
constexpr const char *DSPTIMEOUT = "payment/dsp-timeout";
constexpr const char *CURRENCY_COUNTRIES = "countryCodes"; // historical
constexpr const char *CURRENCY_COUNTRY = "countryCode"; // current

constexpr const char *AppdataFilename = "/appdata";
// used for the default wallet
constexpr const char *DefaultDerivationPath = "m/44'/0'/0'";

enum FileTags {
    WalletId,
    WalletPriority,   // int, maps to PrivacySegment::Priority
    WalletName,       // string. Duplicate of the wallet name
    WalletEncryptionSeed // uint32 (see wallet.h)
};

static P2PNet::Chain s_chain = P2PNet::MainChain;

FloweePay::FloweePay()
    : m_basedir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)),
    m_chain(s_chain)
{
    // make sure the lifetime of the lockedPoolManager exceeds mine (LIFO of globals)
    LockedPoolManager::instance();

    if (m_chain == P2PNet::Testnet4Chain) {
        m_basedir += "/testnet4";
        m_chainPrefix = "bchtest";
    } else {
        m_chainPrefix = "bitcoincash";
    }
    boost::filesystem::create_directories(boost::filesystem::path(m_basedir.toStdString()));

    // make it move to the proper thread.
    connect(this, SIGNAL(loadComplete_priv()), this, SLOT(loadingCompleted()), Qt::QueuedConnection);
#ifdef TARGET_OS_Android
    // on Android, an app is either full screen (active) or inactive and not visible.
    // It is expected we save state as we move to inactive state in order to make the app
    // trivial to kill without loss of data.
    // The above 'aboutToQuit' will not be given enough CPU time to actually save much.
    auto guiApp = qobject_cast<QGuiApplication*>(QCoreApplication::instance());
    assert(guiApp);
    connect(guiApp, &QGuiApplication::applicationStateChanged, this, [=](Qt::ApplicationState state) {
        if (state == Qt::ApplicationInactive || state == Qt::ApplicationSuspended) {
            logInfo() << "App no longer active. Start saving data";
            saveAll();
            p2pNet()->saveData();
            saveData();
        }
        else if (state == Qt::ApplicationActive) {
            /*
            * We are brought back to the foreground.
            *
             * On different iterations of Android the behavior can differ quite substantially
             * when it comes to being allowed to continue using resources while not being active.
             * As such there is no real way to know what being inactive has done to our network
             * connections. They may have been kept alive for whatever time we were
             * not active, they may all have been removed or timed out already.
             *
             * What we'll do is to start the actions again which will check up on our connections
             * and create new ones or download blocks if we need to.
             */
            if (!m_offline)
                p2pNet()->start();
        }
    });
#endif

    QSettings defaultConfig(":/defaults.ini", QSettings::IniFormat);
    m_unit = static_cast<UnitOfBitcoin>(defaultConfig.value(UNIT_TYPE, BCH).toInt());
    m_windowHeight = defaultConfig.value(WINDOW_HEIGHT, -1).toInt();
    m_windowWidth = defaultConfig.value(WINDOW_WIDTH, -1).toInt();
    m_darkSkin = defaultConfig.value(DARKSKIN, true).toBool();
    m_activityShowsBch = defaultConfig.value(ACTIVITYSHOWBCH, false).toBool();
    m_dspTimeout = defaultConfig.value(DSPTIMEOUT, 3000).toInt();
    m_fontScaling = defaultConfig.value(FONTSCALING, 100).toInt();
    m_createStartWallet = defaultConfig.value(CREATE_START_WALLET, false).toBool();

    QSettings appConfig;
    m_unit = static_cast<UnitOfBitcoin>(appConfig.value(UNIT_TYPE, m_unit).toInt());
    m_windowHeight = appConfig.value(WINDOW_HEIGHT, m_windowHeight).toInt();
    m_windowWidth = appConfig.value(WINDOW_WIDTH, m_windowWidth).toInt();
    m_darkSkin = appConfig.value(DARKSKIN, m_darkSkin).toBool();
    m_activityShowsBch = appConfig.value(ACTIVITYSHOWBCH, m_activityShowsBch).toBool();
    m_fontScaling = appConfig.value(FONTSCALING, m_fontScaling).toInt();
    m_dspTimeout = appConfig.value(DSPTIMEOUT, m_dspTimeout).toInt();
    m_hideBalance = appConfig.value(HIDEBALANCE, false).toBool();
    m_prices.reset(new PriceDataProvider(appConfig.value(CURRENCY_COUNTRY).toString()));

    // Update expected chain-height every 5 minutes
    QTimer *timer = new QTimer(this);
    timer->setTimerType(Qt::VeryCoarseTimer);
    timer->start(5 * 60 * 1000);
    connect (timer, SIGNAL(timeout()), this, SIGNAL(expectedChainHeightChanged()));

    QString base;
#if TARGET_OS_Android
    base = QLatin1String("assets:/");
#else
    QDir baseDir(QCoreApplication::applicationDirPath() + "/../share/floweepay/");
    if (baseDir.exists())
        base = baseDir.absolutePath();
    else
        logCritical() << "Warning: No bip39 wordlists found. Looking in:" << baseDir.absolutePath();
#endif
    if (!base.isEmpty()) {
        // add Mnemonic (BIP39) dictionaries.
        struct LangPair { const char *id, *filename; };
        static const LangPair knownPairs[] = {
            {"en", "bip39-english"},
            {"zh-simple", "bip39-chinese_simplified"},
            {"zh-traditional", "bip39-chinese_traditional"},
            {"cs", "bip39-czech"},
            {"fr", "bip39-french"},
            {"it", "bip39-italian"},
            {"ja", "bip39-japanese"},
            {"ko", "bip39-korean"},
            {"pt", "bip39-portuguese"},
            {"es", "bip39-spanish"},
            {0, 0}
        };
        for (int i = 0; knownPairs[i].id; ++i) {
            const LangPair lang = knownPairs[i];
            QString fullPath(base + '/' + lang.filename);
            if (QFile::exists(fullPath))
                m_hdSeedValidator.registerWordList(lang.id, fullPath);
        }
    }

    // forward signal
    connect (&m_notifications, SIGNAL(newBlockMutedChanged()), this, SIGNAL(newBlockMutedChanged()));
    connect (this, &FloweePay::startSaveDate_priv, this, [=]() {
        // As Qt does not allow starting a timer from any thread, we first use a signal
        // to move the request to save to the main thread, after which we schedule it with
        // the singleshot below.
        QTimer::singleShot(1000, this, SLOT(saveData()));
    }, Qt::QueuedConnection);

    connect (QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, [=]() {
        shutdown();
    });
}

// shutting down is done outside of the destructor.
// This is so we have a predictable ordering of shutdown whereas on
// a global destructor the ordering is not locally controlled.
void FloweePay::shutdown()
{
    p2pNet()->shutdown();
    saveData();

    auto *dl = m_downloadManager.get();
    if (dl) // p2pNet follows lazy initialization.
        dl->removeHeaderListener(this);
    for (auto wallet : m_wallets) {
        if (dl) { // p2pNet follows lazy initialization.
            dl->removeDataListener(wallet);
            dl->removeHeaderListener(wallet);
            dl->connectionManager().removePrivacySegment(wallet->segment());
        }
        try {
            wallet->saveWallet();
        } catch (const std::exception &e) {
            logFatal() << e;
        }
    }
    qDeleteAll(m_wallets);
    m_wallets.clear();
}

// static
void FloweePay::selectChain(P2PNet::Chain chain)
{
    s_chain = chain;
}

FloweePay *FloweePay::instance()
{
    static FloweePay s_app;
    return &s_app;
}

void FloweePay::init()
{
    auto dl = p2pNet(); // this wil load the p2p layer.

    QFile in(m_basedir + AppdataFilename);
    Wallet *lastOpened = nullptr;
    if (in.open(QIODevice::ReadOnly)) {
        const auto dataSize = in.size();
        Streaming::BufferPool pool(dataSize);
        in.read(pool.begin(), dataSize);
        Streaming::MessageParser parser(pool.commit(dataSize));
        uint32_t walletEncryptionSeed = 0;
        while (parser.next() == Streaming::FoundTag) {
            if (parser.tag() == WalletId) {
                try {
                    Wallet *w = new Wallet(m_basedir.toStdString(), parser.intData(), walletEncryptionSeed);
                    w->moveToThread(thread());
                    dl->addDataListener(w);
                    dl->addHeaderListener(w);
                    dl->connectionManager().addPrivacySegment(w->segment());
                    m_wallets.append(w);
                    logDebug() << "Found wallet" << w->name() << "with segment ID:" << w->segment()->segmentId();
                    connect (w, &Wallet::encryptionChanged, w, [=]() {
                         // make sure that we get peers for the wallet directly after it gets decrypted
                        if (!m_offline && w->isDecrypted())
                            FloweePay::p2pNet()->addAction<SyncSPVAction>();
                    });
                    lastOpened = w;
                } catch (const std::runtime_error &e) {
                    logWarning() << "Wallet load failed:" << e;
                    lastOpened = nullptr;
                }
                walletEncryptionSeed = 0;
            }
            else if (parser.tag() == WalletPriority) {
                if (lastOpened) {
                    if (parser.isInt()
                            && parser.intData() >= PrivacySegment::First
                            && parser.intData() <= PrivacySegment::OnlyManual) {
                        lastOpened->segment()->setPriority(static_cast<PrivacySegment::Priority>(parser.intData()));
                    }
                    else
                        logWarning() << "Priority out of range";
                }
                else
                    logWarning() << "Priority found, but no wallet to apply it to";
            }
            else if (parser.tag() == WalletName) {
                if (lastOpened && lastOpened->name().isEmpty())
                    lastOpened->setName(QString::fromUtf8(parser.stringData().c_str()));
            }
            else if (parser.tag() == WalletEncryptionSeed) {
                walletEncryptionSeed = static_cast<uint32_t>(parser.longData());
            }
        }
    }

    if (m_wallets.isEmpty() && m_createStartWallet) {
        logInfo() << "Creating startup (initial) wallet";
        // the config is just for QML, delete when we exit scope.
        std::unique_ptr<NewWalletConfig> config(createNewWallet(QLatin1String(DefaultDerivationPath)));
        m_wallets.at(0)->setUserOwnedWallet(false);
        m_wallets.at(0)->segment()->setPriority(PrivacySegment::Last);
        m_wallets.at(0)->setName(tr("Initial Wallet"));
        m_wallets.at(0)->saveWallet();
    }

    emit loadComplete_priv(); // move execution to loadingCompleted, in a Qt thread
}

void FloweePay::loadingCompleted()
{
    for (auto wallet : m_wallets) {
        wallet->performUpgrades();
    }
    if (m_chain == P2PNet::MainChain) {
        m_prices->loadPriceHistory(m_basedir);
        if (!m_offline)
            m_prices->start();
    }
    emit loadComplete();
}

void FloweePay::saveData()
{
    Streaming::BufferPool data(m_wallets.size() * 100);
    Streaming::MessageBuilder builder(data);
    for (auto &wallet : m_wallets) {
        if (wallet->encryptionSeed() != 0)
            builder.add(WalletEncryptionSeed, static_cast<uint64_t>(wallet->encryptionSeed()));
        builder.add(WalletId, wallet->segment()->segmentId());
        builder.add(WalletPriority, wallet->segment()->priority());
        if (!wallet->name().isEmpty()) {
            auto nameData = wallet->name().toUtf8();
            builder.addByteArray(WalletName, nameData.constData(), nameData.size());
        }
    }

    auto buf = builder.buffer();

    // hash the new file and check if its different lest we can skip saving
    const QString filebase = m_basedir + AppdataFilename;
    QFile origFile(filebase);
    if (origFile.open(QIODevice::ReadOnly)) {
        CRIPEMD160 fileHasher;
        auto origContent = origFile.readAll();
        fileHasher.write(origContent.data(), origContent.size());
        char fileHash[CRIPEMD160::OUTPUT_SIZE];
        fileHasher.finalize(fileHash);

        CRIPEMD160 memHasher;
        memHasher.write(buf.begin(), buf.size());
        char memHash[CRIPEMD160::OUTPUT_SIZE];
        memHasher.finalize(memHash);
        if (memcmp(fileHash, memHash, CRIPEMD160::OUTPUT_SIZE) == 0) {
            // no changes, so don't write.
            return;
        }
    }

    QFile out(filebase + "~");
    out.remove(); // avoid overwrite issues.
    if (out.open(QIODevice::WriteOnly)) {
        auto rc = out.write(buf.begin(), buf.size());
        if (rc == -1) {
            logFatal() << "Failed to write. Disk full?";
            // TODO have an app-wide error
            return;
        }
        out.close();
        QFile::remove(filebase);
        if (!out.rename(filebase)) {
            logFatal() << "Failed to rename to" << filebase;
        // TODO have an app-wide error
        }
    } else {
        logFatal() << "Failed to create data file. Permissions issue?";
        // TODO have an app-wide error
    }
}

void FloweePay::saveAll()
{
    for (auto &wallet : m_wallets) {
        wallet->saveWallet();
    }
}

QString FloweePay::basedir() const
{
    return m_basedir;
}

QString FloweePay::platform() const
{
#ifdef TARGET_OS_Android
    return "Android";
#endif
#ifdef TARGET_OS_Linux
    return "Linux";
#endif
    return "unknown"; // TODO for other platforms check which names CMake uses.
}

void FloweePay::startP2PInit()
{
    // start creation of downloadmanager and loading of data in a different thread
    ioService().post(std::bind(&FloweePay::init, this));
}

QString FloweePay::amountToStringPretty(double price) const
{
    QString answer = FloweePay::amountToString(static_cast<qint64>(price), m_unit);
    int c = answer.size();
    while (c > 0) {
        auto k = answer.at(c - 1).unicode();
        if (k == '.' || k == ',') {
            --c;
            break;
        }
        if (k > '0' && k <= '9')
            break;
        --c;
    }
    return answer.left(c);
}

// static
QString FloweePay::amountToString(qint64 price, UnitOfBitcoin unit)
{
    if (unit == Satoshis)
        return QString::number(price);
    QByteArray string(QByteArray::number(std::abs(price)));

    int decimals;
    switch (unit) {
    default:
        decimals = 8;
        break;
    case FloweePay::MilliBCH:
        decimals = 5;
        break;
    case FloweePay::MicroBCH:
    case FloweePay::Bits:
        decimals = 2;
        break;
    }
    const char decimalPoint = QLocale::system().decimalPoint().at(0).unicode();

    int stringLength = string.size();
    int neededSize = std::max(stringLength, decimals) + 1; // 1 for the decimalPoint.
    if (price < 0) // minus sign
        neededSize++;
    if (stringLength <= decimals) // add a zero in front of the decimalPoint too.
        neededSize++;
    string.resize(neededSize);
    char *str = string.data();
    memcpy(str + string.size() - stringLength, str, stringLength); // move to be right-aligned
    for (int i = string.size() - stringLength; i > 0; --i) { str[i - 1] = '0'; } // pad zeros on left

    // insert the actual decimal point. We need to move the part in front of it back to the left to make space.
    for (int i = 0; i < string.size() - decimals; ++i) {
        str[i - 1] = str[i];
    }
    str[string.size() - decimals - 1] = decimalPoint;
    if (price < 0)
        *str = '-';
    return QString::fromLatin1(str);
}

QString FloweePay::formatDate(QDateTime date) const
{
    static QString format = QLocale::system().dateFormat(QLocale::ShortFormat);
    if (!date.isValid() || date.isNull())
        return QString();

    const QDateTime now = QDateTime::currentDateTime();
    if (now > date) {
        // use the 'yesterday' style if the date is reasonably close.
        const auto days = date.daysTo(now);
        if (days == 0)
            return tr("Today");
        if (days == 1)
            return tr("Yesterday");
        if (days < 9) // return day of the week
            return date.toString("dddd");
    }

    if (date.date().year() == QDate::currentDate().year()) {
        static QString shortFormat;
        if (shortFormat.isEmpty()) {
            // We basically just need to know if this locale has months first or not
            int m = format.indexOf('m');
            if (m == -1)
                m = format.indexOf('M');
            int d = format.indexOf('d');
            if (d == -1)
                d = format.indexOf('D');
            if (m < d)
                shortFormat = "d MMM";
            else
                shortFormat = "MMM d";
        }
        return date.toString(shortFormat);
    }
    return date.toString(format);
}

QString FloweePay::formatDateTime(QDateTime date) const
{
    static QString format = QLocale::system().dateTimeFormat(QLocale::ShortFormat);
    static QString timeFormat = QLocale::system().timeFormat(QLocale::ShortFormat);
    if (!date.isValid() || date.isNull())
        return QString();

    const QDateTime now = QDateTime::currentDateTime();
    // instead of simply checking if \a date is in the past, allow for a bit of
    // imprecision based on the fact that blocks are not going to have the precise
    // timestamps (due to how mining works) and they can be slightly in the future.
    // So to avoid a timestamp that is up to a minute in the future being displayed
    // with a full ISO date instead of 'now', we need to have the next line be a
    // bit more lenient.
    if (date.secsTo(now) > -60) { // in the past, or at most 1 min in the future.
        // use the 'yesterday' style if the date is reasonably close.
        const auto secs = date.secsTo(now);
        if (secs < 24 * 60) {
            const int mins = (secs + 24) / 60;
            if (mins <= 0)
                return tr("Now", "timestamp");
            return tr("%1 minutes ago", "relative time stamp", mins).arg(mins);
        }
        if (secs < 18 * 60 * 60) {
            if (secs < 46 * 60)
                return tr("½ hour ago", "timestamp");
            const int hours = (secs + 900) / 3600;
            return tr("%1 hours ago", "timestamp", hours).arg(hours);
        }
        const auto days = date.daysTo(now);
        if (days == 0)
            return tr("Today") + " " + date.toString(timeFormat);
        if (days == 1)
            return tr("Yesterday") + " " + date.toString(timeFormat);
        if (days < 9) // return day of the week
            return date.toString("dddd " + timeFormat);
    }
    return date.toString(format);
}

Wallet *FloweePay::createWallet(const QString &name)
{
    auto dl = p2pNet();

    quint16 id;
    while (true) {
        id = GetRandInt(0xFFFF);
        QString dir = QString("/wallet-%1").arg(id);
        if (!QFileInfo::exists(m_basedir + dir))
            break;
    }

    Wallet *w = Wallet::createWallet(m_basedir.toStdString(), id, name);
    dl->addDataListener(w);
    dl->addHeaderListener(w);
    dl->connectionManager().addPrivacySegment(w->segment());
    w->moveToThread(thread());
    m_wallets.append(w);

    emit startSaveDate_priv(); // schedule a save of the m_wallets list
    return w;
}

P2PNet::Chain FloweePay::chain() const
{
    return m_chain;
}

void FloweePay::copyToClipboard(const QString &text)
{
    QGuiApplication::clipboard()->setText(text);
}

void FloweePay::openInExplorer(const QString &text)
{
    if (text.size() == 64) { // assume this is a txid
        QDesktopServices::openUrl(QUrl("https://blockchair.com/bitcoin-cash/transaction/" + text));
        return;
    }
    // Add maybe other types?
}

FloweePay::UnitOfBitcoin FloweePay::unit() const
{
    return m_unit;
}

void FloweePay::setUnit(const UnitOfBitcoin &unit)
{
    if (m_unit == unit)
        return;
    m_unit = unit;
    QSettings appConfig;
    appConfig.setValue(UNIT_TYPE, m_unit);
    emit unitChanged();
}

int FloweePay::headerChainHeight() const
{
    if (!m_downloadManager.get())
        return 0;
    return m_downloadManager->blockHeight();
}

int FloweePay::expectedChainHeight() const
{
    if (!m_downloadManager.get())
        return 0;
    return m_downloadManager->blockchain().expectedBlockHeight();
}

int FloweePay::chainHeight()
{
    const int hch = headerChainHeight();
    if (!m_gotHeadersSyncedOnce) {
        const int expected = expectedChainHeight();
        const int behind = expected - hch; // num blocks we are behind theoretical height
        if (behind > 6) // don't report expected when variance could explain the diff
            return expected;
    }
    return headerChainHeight();
}

void FloweePay::setHeaderSyncHeight(int newHeight)
{
    if (m_wallets.count() > 1) {
        for (auto *wallet : m_wallets) {
            if (!wallet->userOwnedWallet()) {
                // we create a default wallet on first startup but we ignore that one
                // should the user create a new one directly afterwards.
                // We can't simply delete it as the QR code was visible, so we have to wait a while
                // to make sure no funds have been sent there before we stop finding peers for it.

                if (wallet->segment()->priority() != PrivacySegment::OnlyManual) {
                    // not yet archived
                    auto h = wallet->walletCreatedHeight();
                    if (h < 0) break; // should not happen

                    if (newHeight - h > 2100) // its been more than 2 weeks. Archive.
                        wallet->segment()->setPriority(PrivacySegment::OnlyManual);
                }
                break;
            }
        }
    }

    emit headerChainHeightChanged();
}

void FloweePay::headerSyncComplete()
{
    m_gotHeadersSyncedOnce = true;
    emit headerChainHeightChanged();
}

bool FloweePay::darkSkin() const
{
    return m_darkSkin;
}

void FloweePay::setDarkSkin(bool darkSkin)
{
    if (m_darkSkin == darkSkin)
        return;
    m_darkSkin = darkSkin;
    emit darkSkinChanged();
    QSettings appConfig;
    appConfig.setValue(DARKSKIN, m_darkSkin);
}

int FloweePay::windowHeight() const
{
    return m_windowHeight;
}

void FloweePay::setWindowHeight(int windowHeight)
{
    if (m_windowHeight == windowHeight)
        return;
    m_windowHeight = windowHeight;
    QSettings appConfig;
    appConfig.setValue(WINDOW_HEIGHT, m_windowHeight);
    emit windowHeightChanged();
}

int FloweePay::windowWidth() const
{
    return m_windowWidth;
}

void FloweePay::setWindowWidth(int windowWidth)
{
    if (windowWidth == m_windowWidth)
        return;
    m_windowWidth = windowWidth;
    emit windowWidthChanged();
    QSettings appConfig;
    appConfig.setValue(WINDOW_WIDTH, m_windowWidth);
}

uint32_t FloweePay::walletStartHeightHint() const
{
    if (m_downloadManager->isChainUpToDate())
        return m_downloadManager->blockHeight();

    // return a massively too high number that the wallet will
    // interpret as seconds and match it to the block time to resolve a
    // hight when the headers are synched.
    return time(nullptr);
}

bool FloweePay::activityShowsBch() const
{
    return m_activityShowsBch;
}

void FloweePay::setActivityShowsBch(bool newActivityShowsBch)
{
    if (m_activityShowsBch == newActivityShowsBch)
        return;
    m_activityShowsBch = newActivityShowsBch;
    emit activityShowsBchChanged();
    QSettings appConfig;
    appConfig.setValue(ACTIVITYSHOWBCH, m_activityShowsBch);
}

int FloweePay::fontScaling() const
{
    return m_fontScaling;
}

void FloweePay::setFontScaling(int newFontScaling)
{
    if (m_fontScaling == newFontScaling)
        return;
    m_fontScaling = newFontScaling;
    emit fontScalingChanged();
    QSettings appConfig;
    appConfig.setValue(FONTSCALING, m_fontScaling);
}

PriceDataProvider *FloweePay::prices() const
{
    return m_prices.get();
}

bool FloweePay::isOffline() const
{
    return m_offline;
}

void FloweePay::setOffline(bool offline)
{
    m_offline = offline;
}

void FloweePay::startNet()
{
    if (m_offline)
        return;
    p2pNet()->start(); // lets go!
}

bool FloweePay::newBlockMuted() const
{
    return m_notifications.newBlockMuted();
}

void FloweePay::setNewBlockMuted(bool mute)
{
    m_notifications.setNewBlockMuted(mute);
}

CameraController *FloweePay::cameraController()
{
    return m_cameraController;
}

void FloweePay::setCameraController(CameraController *cc)
{
    m_cameraController = cc;
}

QString FloweePay::version() const
{
    return QCoreApplication::instance()->applicationVersion();
}

QString FloweePay::libsVersion() const
{
    return QString("%1.%2.%3")
            .arg(CLIENT_VERSION_MAJOR)
            .arg(QString::number(CLIENT_VERSION_MINOR), 2, '0')
            .arg(CLIENT_VERSION_REVISION);
}

bool FloweePay::hideBalance() const
{
    return m_hideBalance;
}

void FloweePay::setHideBalance(bool hideBalance)
{
    if (m_hideBalance == hideBalance)
        return;
    m_hideBalance = hideBalance;
    emit hideBalanceChanged();
    QSettings appConfig;
    appConfig.setValue(HIDEBALANCE, m_hideBalance);
}

NewWalletConfig* FloweePay::createImportedWallet(const QString &privateKey, const QString &walletName, const QDateTime &date)
{
    const int height = p2pNet()->blockchain().blockHeightAtTime(date.toSecsSinceEpoch());
    return createImportedWallet(privateKey, walletName, height);
}

NewWalletConfig* FloweePay::createImportedWallet(const QString &privateKey, const QString &walletName, int startHeight)
{
    auto wallet = createWallet(walletName);
    wallet->setSingleAddressWallet(true);
    if (startHeight <= 1)
        startHeight = m_chain == P2PNet::MainChain ? 550000 : 1000;
    wallet->addPrivateKey(privateKey.trimmed().remove('\n'), startHeight);
    emit walletsChanged();
    if (!m_offline)
        p2pNet()->addAction<SyncSPVAction>(); // make sure that we get peers for the new wallet.

    return new NewWalletConfig(wallet);
}

QObject *FloweePay::researchAddress(const QString &address, QObject *parent)
{
    CashAddress::Content c = CashAddress::decodeCashAddrContent(address.toStdString(), m_chainPrefix);
    if (c.hash.empty() || c.type != CashAddress::PUBKEY_TYPE) {
        logWarning() << "researchAddress() only works with a propertly formatted cash-address!";
        return nullptr;
    }
    const KeyId key(reinterpret_cast<char *>(c.hash.data()));

    // if we don't know the address, return a nullptr
    AddressInfo *info = nullptr;

    for (const auto *wallet : qAsConst(m_wallets)) {
        int privKeyId = wallet->findPrivKeyId(key);
        if (privKeyId != -1) {
            info = new AddressInfo(address, parent);
            auto details = wallet->fetchKeyDetails(privKeyId);
            info->setCoins(details.coins);
            info->setHistoricalCoins(details.historicalCoins);
            info->setSaldo(details.saldo);
            info->setAccountName(wallet->name());
            info->setAccountId(wallet->segment()->segmentId());
            break;
        }
    }
    return info;
}

NewWalletConfig* FloweePay::createImportedHDWallet(const QString &mnemonic, const QString &password, const QString &derivationPathStr, const QString &walletName, const QDateTime &date)
{
    const int height = p2pNet()->blockchain().blockHeightAtTime(date.toSecsSinceEpoch());
    return createImportedHDWallet(mnemonic, password, derivationPathStr, walletName, height);
}

NewWalletConfig* FloweePay::createImportedHDWallet(const QString &mnemonic, const QString &password, const QString &derivationPathStr, const QString &walletName, int startHeight)
{
    auto wallet = createWallet(walletName);
    try {
        std::vector<uint32_t> derivationPath = HDMasterKey::deriveFromString(derivationPathStr.toStdString());
        if (startHeight <= 1)
            startHeight = m_chain == P2PNet::MainChain ? 550000 : 1000;
        wallet->createHDMasterKey(mnemonic.trimmed().remove('\n'), password.trimmed().remove('\n'),
                                  derivationPath, startHeight);
        emit walletsChanged();
        if (!m_offline)
            p2pNet()->addAction<SyncSPVAction>(); // make sure that we get peers for the new wallet.

        return new NewWalletConfig(wallet);
    } catch (const std::exception &e) {
        logFatal() << "Failed to parse user provided data due to:" << e;
        return nullptr;
    }
}

bool FloweePay::checkDerivation(const QString &path) const
{
    try {
        auto vector = HDMasterKey::deriveFromString(path.toStdString());
        return true;
    } catch (...) {
        return false;
    }
}

WalletEnums::StringType FloweePay::identifyString(const QString &string) const
{
    const QString string_ = string.trimmed().remove('\n');
    const std::string s = string_.toStdString();
    if (string_.isEmpty()) {
        m_hdSeedValidator.clearSelectedLanguage();
        return WalletEnums::Unknown;
    }

    CBase58Data legacy;
    if (legacy.SetString(s)) {
        if ((m_chain == P2PNet::MainChain && legacy.isMainnetPkh())
            || (m_chain == P2PNet::Testnet4Chain && legacy.isTestnetPkh()))
            return WalletEnums::LegacyPKH;
        if ((m_chain == P2PNet::MainChain && legacy.isMainnetSh())
            || (m_chain == P2PNet::Testnet4Chain && legacy.isTestnetSh()))
            return WalletEnums::LegacySH;
        if ((m_chain == P2PNet::MainChain && legacy.isMainnetPrivKey())
            || (m_chain == P2PNet::Testnet4Chain && legacy.isTestnetPrivKey()))
            return WalletEnums::PrivateKey;
    }

    CashAddress::Content c = CashAddress::decodeCashAddrContent(s, m_chainPrefix);
    if (!c.hash.empty()) {
        if (c.type == CashAddress::PUBKEY_TYPE)
            return WalletEnums::CashPKH;
        if (c.type == CashAddress::SCRIPT_TYPE)
            return WalletEnums::CashSH;
    }

    try {
        int firstWord = -2;
        int space = -1;
        do {
            ++space;
            int space2 = string.indexOf(' ', space);
            auto word = string.mid(space, space2 - space);
            if (!word.isEmpty()) {
                int index = m_hdSeedValidator.findWord(word);
                if (firstWord == -2) {
                    firstWord = index;
                    if (index != -1) {
                        auto validity = m_hdSeedValidator.validateMnemonic(string_, index);
                        if (validity == Mnemonic::Valid)
                            return WalletEnums::CorrectMnemonic;
                    }
                    else { // not a recognized word
                        break;
                    }
                }
                else if (index == -1) { // a not-first-word failed the lookup.
                    if (space2 != -1) // this is the last word, don't highlight while writing.
                        return WalletEnums::PartialMnemonicWithTypo;
                    break;
                }
                // if we get to this point in the loop then we have a real word that we found in the dictionary.
                // Lets continue checking words and check if the rest of the words are part of the lexicon too.
            }
            space = space2;
        } while (space != -1);
        if (firstWord >= 0)
            return WalletEnums::PartialMnemonic;
    } catch (const std::exception &e) {
        // probably deployment issues (faulty word list)
        logFatal() << e;
        return WalletEnums::MissingLexicon;
    }
    return WalletEnums::Unknown;
}

NewWalletConfig* FloweePay::createNewBasicWallet(const QString &walletName)
{
    auto wallet = createWallet(walletName);
    wallet->createNewPrivateKey(walletStartHeightHint());
    emit walletsChanged();
    if (!m_offline)
        p2pNet()->addAction<SyncSPVAction>();
    return new NewWalletConfig(wallet);
}

NewWalletConfig* FloweePay::createNewWallet(const QString &derivationPath, const QString &password, const QString &walletName)
{
    // special case the first user-created wallet.
    // If the user creates a new wallet that is identical to the one we auto-created, reuse that one.
    const bool haveOneHiddenWallet = m_wallets.size() == 1 && !m_wallets.first()->userOwnedWallet();
    if (haveOneHiddenWallet) {
        auto wallet = m_wallets.first();
        if (wallet->isHDWallet() && derivationPath == wallet->derivationPath() && password == wallet->hdWalletMnemonicPwd()) {
            wallet->setUserOwnedWallet(true);
            if (!walletName.isEmpty())
                wallet->setName(walletName);
            // little hacky to make listeners realize we really changed the wallet.
            m_wallets.clear();
            emit walletsChanged();
            m_wallets.append(wallet);
            emit walletsChanged();
            return new NewWalletConfig(wallet);
        }
    }

    auto wallet = createWallet(walletName);
    std::vector<uint8_t> seed(16); // 12 word seed
    RandAddSeedPerfmon();
    GetRandBytes(seed.data(), seed.size());
    auto mnemonic = m_hdSeedValidator.generateMnemonic(seed, "en");
    std::vector<uint32_t> dp = HDMasterKey::deriveFromString(derivationPath.toStdString());
    wallet->createHDMasterKey(mnemonic, password, dp, walletStartHeightHint());
    emit walletsChanged();
    if (!m_offline)
        p2pNet()->addAction<SyncSPVAction>(); // make sure that we get peers for the new wallet.

    return new NewWalletConfig(wallet);
}

QString FloweePay::unitName() const
{
    return  nameOfUnit(m_unit);
}

QString FloweePay::nameOfUnit(FloweePay::UnitOfBitcoin unit) const
{
    switch (unit) {
    case FloweePay::BCH:
        if (m_chain == P2PNet::MainChain)
            return QLatin1String("BCH");
        else
            return QLatin1String("tBCH");
    case FloweePay::MilliBCH:
        if (m_chain == P2PNet::MainChain)
            return QLatin1String("mBCH");
        else
            return QLatin1String("m-tBCH");
    case FloweePay::MicroBCH:
        if (m_chain == P2PNet::MainChain)
            return QString("µBCH");
        else
            return QString("µ-tBCH");
    case FloweePay::Bits:
        if (m_chain == P2PNet::MainChain)
            return QLatin1String("bits");
        else
            return QLatin1String("tbits");
    case FloweePay::Satoshis:
        if (m_chain == P2PNet::MainChain)
            return QLatin1String("sats");
        else
            return QLatin1String("tsats");
    default:
        return QString();
    }
}

void FloweePay::setCountry(const QString &countrycode)
{
    m_prices->setCountry(countrycode);
    QSettings appConfig;
    appConfig.setValue(CURRENCY_COUNTRY, countrycode);
    auto list = recentCountries();
    if (!list.isEmpty() && list.first() == countrycode)
        return;
    list.removeAll(countrycode); // avoid duplicates
    list.insert(0, countrycode);
    if (list.size() > 5)
        list.resize(5);
    appConfig.setValue(CURRENCY_COUNTRIES, list);
}

QStringList FloweePay::recentCountries() const
{
    QSettings appConfig;
    auto list = appConfig.value(CURRENCY_COUNTRIES).toStringList();
    if (list.isEmpty()) {
        QSettings defaultConfig(":/defaults.ini", QSettings::IniFormat);
        list = defaultConfig.value(CURRENCY_COUNTRIES).toStringList();
    }
    return list;
}

int FloweePay::unitAllowedDecimals() const
{
    switch (m_unit) {
    case FloweePay::BCH:
        return 8;
    case FloweePay::MilliBCH:
        return 5;
    case FloweePay::MicroBCH:
    case FloweePay::Bits:
        return 2;
    case FloweePay::Satoshis:
        return 0;
    default:
        Q_ASSERT(false);
        return 0;
    }
}

QList<Wallet *> FloweePay::wallets() const
{
    return m_wallets;
}

DownloadManager *FloweePay::p2pNet()
{
    if (m_downloadManager == nullptr) {
        m_downloadManager.reset(new DownloadManager(ioService(), m_basedir.toStdString(), m_chain));
        m_downloadManager->addHeaderListener(this);
        m_downloadManager->notifications().addListener(&m_notifications);

        QSettings defaultConfig(":/defaults.ini", QSettings::IniFormat);
        QString useragent = defaultConfig.value(USERAGENT, "Flowee Pay Wallet").toString();
        m_downloadManager->connectionManager().setUserAgent(useragent.toStdString());
        emit headerChainHeightChanged();
        emit expectedChainHeightChanged();
    }
    return m_downloadManager.get();
}

int FloweePay::dspTimeout() const
{
    return m_dspTimeout;
}

void FloweePay::setDspTimeout(int milliseconds)
{
    if (milliseconds == m_dspTimeout)
        return;
    m_dspTimeout = milliseconds;
    emit dspTimeoutChanged();
    QSettings appConfig;
    appConfig.setValue(DSPTIMEOUT, m_dspTimeout);
}

const std::string &chainPrefix()
{
    return FloweePay::instance()->chainPrefix();
}

QString renderAddress(const KeyId &pubkeyhash)
{
    CashAddress::Content c;
    c.type = CashAddress::PUBKEY_TYPE;
    c.hash = std::vector<uint8_t>(pubkeyhash.begin(), pubkeyhash.end());
    const std::string &chainPref = FloweePay::instance()->chainPrefix();
    auto s = CashAddress::encodeCashAddr(chainPref, c);
    const auto size = chainPref.size();
    return QString::fromLatin1(s.c_str() + size + 1, s.size() - size -1); // the 1 is for the colon
}
