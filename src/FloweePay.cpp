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
#include "FloweePay.h"
#include "Wallet.h"
#include "NewWalletConfig.h"
#include "AddressInfo.h"
#include "PriceDataProvider.h"

#include <base58.h>
#include <cashaddr.h>
#include <streaming/MessageParser.h>
#include <streaming/BufferPools.h>
#include <streaming/MessageBuilder.h>
#include <random.h>
#include <config/flowee-config.h>
#include <crypto/ripemd160.h>
#include <crypto/sha256.h>
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
#include <QThread>
#include <QTimer>
#include <QUrl>

#ifdef TARGET_OS_Android
# include <QJniObject>
#endif

#include <system_error>
#include <filesystem>
#include <fstream>
#include <utility>

constexpr const char *UNIT_TYPE = "unit";
constexpr const char *CREATE_START_WALLET = "create-start-wallet";
constexpr const char *WINDOW_WIDTH = "window/width";
constexpr const char *WINDOW_HEIGHT = "window/height";
constexpr const char *FONTSCALING = "window/font-scaling";
constexpr const char *DARKSKIN = "darkSkin";
constexpr const char *DARKSKIN_FROM_PLATFORM = "darkSkin-from-platform";
constexpr const char *ACTIVITYSHOWBCH = "activity-show-bch";
constexpr const char *HIDEBALANCE = "hide-balance";
constexpr const char *USERAGENT = "net/useragent";
constexpr const char *DSPTIMEOUT = "payment/dsp-timeout";
constexpr const char *CURRENCY_COUNTRIES = "countryCodes"; // historical
constexpr const char *CURRENCY_COUNTRY = "countryCode"; // current
constexpr const char *PRIVATE_MODE = "private-mode";

constexpr const char *AppdataFilename = "/appdata";
// used for the default wallet
constexpr const char *DefaultDerivationPath = "m/44'/0'/0'";

enum FileTags {
    WalletId,
    WalletPriority,   // int, maps to PrivacySegment::Priority
    WalletName,       // string. Duplicate of the wallet name
    WalletEncryptionSeed, // uint32 (see wallet.h)
    WalletSetting_CountBalance, // bool, if we count balance in app-total
    WalletSetting_IsPrivate, // bool, will be hidden in priate mode
    WalletSetting_FiatInstaPayEnabled, // bool
    WalletSetting_FiatInstaPayLimitCurrency, // string, ISO-currency-code
    WalletSetting_FiatInstaPayLimit, // int, cents. Has to be directly behind the currency.

    // security features
    AppProtectionType = 20, // see enum FloweePay::ApplicationProtection
    AppProtectionHash,      // the hash of the password in case type is AppPassword
};

static P2PNet::Chain s_chain = P2PNet::MainChain;

namespace {
QList<QStringView> splitString(const QString &input)
{
    QList<QStringView> words;
    const QStringView stringView(input);
    int wordStart = -1;
    for (int i = 0; i < input.length(); ++i) {
        if (input.at(i).isSpace()) {
            if (wordStart != -1) {
                words.append(stringView.sliced(wordStart, i - wordStart));
                wordStart = -1;
            }
        }
        else if (wordStart == -1) {
            wordStart = i;
        }
    }
    if (wordStart != -1)
        words.append(stringView.sliced(wordStart, stringView.length() - wordStart));

    return words;
}

QString joinWords(const QList<QStringView> &words, bool lowercaseFirstWord)
{
    QString string;
    for (const auto word : words) {
        if (string.isEmpty()) {
            if (lowercaseFirstWord) {
                string = word.toString().toLower();
                continue;
            }
        }
        else {
            string += QLatin1Char(' ');
        }
        string += word.toString();
    }

    return string;
}


}

FloweePay::FloweePay()
    : m_chain(s_chain),
    m_basedir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
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
            if (m_sleepStart.isNull()) {
                logInfo() << "App no longer active. Start saving data";
                m_sleepStart = QDateTime::currentDateTimeUtc();
                saveAll();
                p2pNet()->saveData();
                saveData();
            }
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
             * and create new ones if we need them.
             */
            if (!m_offline && m_loadingCompleted)
                p2pNet()->start();

            if (m_appProtection == AppUnlocked
                    && m_sleepStart.addSecs(60 * 10) < QDateTime::currentDateTimeUtc()) {
                // re-lock the app after 10 minutes of not being in the front.
                setAppProtection(AppPassword);
            }
            m_sleepStart = QDateTime();
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

    /*
     * Darkskin and darkskinFromSystem were introduced one after the other.
     * People that have darkskin stored, but not darkskinFromSystem thus need to
     * be migrated from the old setup to the new.
     * For migrating users, the darkSkinFromSystem is set to false. For
     * new users (i.e. everyone else), it defaults to true.
     */
     bool defaultPlatform = true;
     if (!appConfig.value(DARKSKIN_FROM_PLATFORM).isValid() && appConfig.value(DARKSKIN).isValid())
         defaultPlatform = false;

    m_darkSkin = appConfig.value(DARKSKIN, m_darkSkin).toBool();
    setSkinFollowsPlatform(appConfig.value(DARKSKIN_FROM_PLATFORM, defaultPlatform).toBool());
    m_activityShowsBch = appConfig.value(ACTIVITYSHOWBCH, m_activityShowsBch).toBool();
    m_fontScaling = appConfig.value(FONTSCALING, m_fontScaling).toInt();
    m_dspTimeout = appConfig.value(DSPTIMEOUT, m_dspTimeout).toInt();
    m_hideBalance = appConfig.value(HIDEBALANCE, false).toBool();
    m_privateMode = appConfig.value(PRIVATE_MODE, false).toBool();
    m_prices.reset(new PriceDataProvider(appConfig.value(CURRENCY_COUNTRY).toString()));

    // Update expected chain-height every 5 minutes
    QTimer *timer = new QTimer(this);
    timer->setTimerType(Qt::VeryCoarseTimer);
    timer->start(5 * 60 * 1000);
    connect (timer, SIGNAL(timeout()), this, SIGNAL(expectedChainHeightChanged()));

    QString base;
#ifdef TARGET_OS_Android
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

    // load the AppData file and only fetch the AppProtection tags.
    // the rest of the data is used in init() when there is a valid p2pNet loaded.
    QFile in(m_basedir + AppdataFilename);
    if (in.open(QIODevice::ReadOnly)) {
        const auto dataSize = in.size();
        auto pool = Streaming::pool(dataSize);
        in.read(pool->begin(), dataSize);
        Streaming::MessageParser parser(pool->commit(dataSize));
        while (parser.next() == Streaming::FoundTag) {
            switch (parser.tag()) {
            case AppProtectionType:
                m_appProtection = static_cast<ApplicationProtection>(parser.intData());
                break;
            case AppProtectionHash:
                m_appProtectionHash = parser.uint256Data();
                break;
            default:
                break;
            }
        }
        in.close();
    }

    // forward signal
    connect (&m_notifications, SIGNAL(newBlockMutedChanged()), this, SIGNAL(newBlockMutedChanged()));
    connect (this, &FloweePay::startSaveData_priv, this, [=]() {
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

void FloweePay::sendTransactionNotification(const P2PNet::Notification &notification)
{
    auto *me = FloweePay::instance();
    auto configs = me->m_accountConfigs;
    auto i = configs.find(notification.privacySegment);
    // don't broadcast notifications of private wallets when private mode is enabled
    if (me->privateMode() && (i == configs.end() || i->privateWallet))
        return;

    me->p2pNet()->notifications().notifyNewTransaction(notification);
}

void FloweePay::init()
{
    auto dl = p2pNet(); // this wil load the p2p layer.
    dl->connectionManager().peerAddressDb().pardonOldCrimes();

    QFile in(m_basedir + AppdataFilename);
    Wallet *lastOpened = nullptr;
    QString currencyCode; // for wallet config
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
                    m_accountConfigs.insert(w->segment()->segmentId(), {});
                    connectToWallet(w);
                    logDebug() << "Found wallet" << w->name() << "with segment ID:" << w->segment()->segmentId();
                    lastOpened = w;

                    // help make sure newly started wallets start synching.
                    w->checkHeaderSyncComplete(p2pNet()->blockchain());
                } catch (const std::runtime_error &e) {
                    logWarning() << "Wallet load failed:" << e;
                    lastOpened = nullptr;
                }
                walletEncryptionSeed = 0;
                currencyCode.clear();
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
            else if (parser.tag() == WalletSetting_CountBalance) {
                if (lastOpened)
                    m_accountConfigs[lastOpened->segment()->segmentId()].countBalance = parser.boolData();
                else
                    logWarning() << "Setting seen before walletId";
            }
            else if (parser.tag() == WalletSetting_IsPrivate) {
                if (lastOpened)
                    m_accountConfigs[lastOpened->segment()->segmentId()].privateWallet = parser.boolData();
                else
                    logWarning() << "Setting seen before walletId";
            }
            else if (parser.tag() == WalletSetting_FiatInstaPayLimitCurrency) {
                if (lastOpened)
                    currencyCode = QString::fromUtf8(parser.stringData());
                else
                    logWarning() << "Setting seen before walletId";
            }
            else if (parser.tag() == WalletSetting_FiatInstaPayEnabled) {
                if (lastOpened)
                    m_accountConfigs[lastOpened->segment()->segmentId()].allowInstaPay = parser.boolData();
                else
                    logWarning() << "Setting seen before walletId";
            }
            else if (parser.tag() == WalletSetting_FiatInstaPayLimit) {
                if (lastOpened && !currencyCode.isEmpty())
                    m_accountConfigs[lastOpened->segment()->segmentId()].fiatInstaPayLimits[currencyCode]
                        = parser.intData();
                else
                    logWarning() << "Setting seen before walletId or currencyCode";
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
    m_loadingCompleted = true;
    if (m_appProtection != AppPassword) { // in that case, wait for password.
        assert(!m_loadCompletEmitted);
        m_loadCompletEmitted = true;
        emit loadComplete();
    }
}

void FloweePay::saveData()
{
    auto data = std::make_shared<Streaming::BufferPool>(m_wallets.size() * 100);
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

        // each wallet should have a config file, lets save the content
        auto conf = m_accountConfigs.find(wallet->segment()->segmentId());
        assert(conf != m_accountConfigs.end());
        builder.add(WalletSetting_CountBalance, conf->countBalance);
        builder.add(WalletSetting_IsPrivate, conf->privateWallet);
        builder.add(WalletSetting_FiatInstaPayEnabled, conf->allowInstaPay);
        QMapIterator<QString,int> iter(conf->fiatInstaPayLimits);
        while (iter.hasNext()) {
            iter.next();
            builder.add(WalletSetting_FiatInstaPayLimitCurrency, iter.key().toStdString());
            builder.add(WalletSetting_FiatInstaPayLimit, iter.value());
        }
    }

    auto protection = m_appProtection;
    if (protection == AppUnlocked) // thats an in-memory only state.
        protection = AppPassword;
    builder.add(AppProtectionType, protection);
    if (protection == AppPassword) {
        assert(!m_appProtectionHash.IsNull()); // it would be impossible to unlock if so.
        builder.add(AppProtectionHash, m_appProtectionHash);
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

    try {
        std::string filebaseStr(filebase.toStdString());
        std::ofstream out(filebaseStr + "~");
        out.write(buf.begin(), buf.size());
        out.flush();
        out.close();
        std::filesystem::rename(filebaseStr + "~", filebaseStr);
    } catch (const std::exception &e) {
        logFatal() << "Failed to create data file. Permissions issue?" << e;
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

QString FloweePay::formatBlockTime(int blockHeight) const
{
    // wrap this for convenience and also ensure that we never return an insanely old
    // date (1970) just because we lack blockheader data.
    return formatDateTime(QDateTime::fromSecsSinceEpoch(std::max<uint32_t>(1250000000,
            m_downloadManager->blockchain().block(blockHeight).nTime)));
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
    m_accountConfigs.insert(id, {});
    connectToWallet(w);

    emit startSaveData_priv(); // schedule a save of the m_wallets list
    return w;
}

P2PNet::Chain FloweePay::chain() const
{
    return m_chain;
}

QString FloweePay::qchainPrefix() const
{
    return QString("%1:").arg(QString::fromStdString(m_chainPrefix));
}

void FloweePay::copyToClipboard(const QString &text)
{
    QGuiApplication::clipboard()->setText(text);
}

void FloweePay::openInExplorer(const QString &txid)
{
    QDesktopServices::openUrl(QUrl("https://blockchair.com/search?q=" + txid));
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

void FloweePay::connectToWallet(Wallet *wallet)
{
    connect (wallet, &Wallet::encryptionChanged, wallet, [=]() {
         // make sure that we get peers for the wallet directly after it gets decrypted
        if (!m_offline && wallet->isDecrypted())
            FloweePay::p2pNet()->addAction<SyncSPVAction>();
    });
    connect (wallet, &Wallet::encryptionSeedChanged, wallet, [=]() {
        // the encryption seed is saved in the wallet-list.
        // Save as soon as the data changed.
        saveData();
        }, Qt::QueuedConnection);
}

bool FloweePay::skinFollowsPlatform() const
{
    return m_skinFollowsPlatform;
}

void FloweePay::setSkinFollowsPlatform(bool newSkinFollowsPlatform)
{
    if (m_skinFollowsPlatform == newSkinFollowsPlatform)
        return;
    m_skinFollowsPlatform = newSkinFollowsPlatform;
    emit skinFollowsPlatformChanged();
    QSettings appConfig;
    appConfig.setValue(DARKSKIN_FROM_PLATFORM, m_skinFollowsPlatform);

#ifdef TARGET_OS_Android
    if (newSkinFollowsPlatform) {
        auto context = QJniObject(QNativeInterface::QAndroidApplication::context());
        // In Java:
        //   context.getResources().getConfiguration().uiMode
        auto resources = context.callObjectMethod("getResources",
                "()Landroid/content/res/Resources;");
        auto config = resources.callObjectMethod("getConfiguration",
                "()Landroid/content/res/Configuration;");
        auto uiMode = config.getField<jint>("uiMode");
        constexpr int UI_MODE_NIGHT_MASK = 0x30;
        constexpr int UI_MODE_NIGHT_YES = 0x20;
        const bool dark = (uiMode & UI_MODE_NIGHT_MASK) == UI_MODE_NIGHT_YES;
        setDarkSkin(dark);
    }
#endif
}

QString FloweePay::paymentProtocolRequest() const
{
    return m_paymentProtocolRequest;
}

void FloweePay::setPaymentProtocolRequest(const QString &ppr)
{
    if (m_paymentProtocolRequest == ppr)
        return;
    m_paymentProtocolRequest = ppr;
    emit paymentProtocolRequestChanged();
}

FloweePay::ApplicationProtection FloweePay::appProtection() const
{
    return m_appProtection;
}

void FloweePay::setAppProtection(ApplicationProtection prot)
{
    if (m_appProtection == prot)
        return;
    m_appProtection = prot;
    emit appProtectionChanged();
}

bool FloweePay::privateMode() const
{
    return m_privateMode;
}

void FloweePay::setPrivateMode(bool newPrivateMode)
{
    if (m_privateMode == newPrivateMode)
        return;
    m_privateMode = newPrivateMode;
    emit privateModeChanged();
    QSettings appConfig;
    appConfig.setValue(PRIVATE_MODE, m_privateMode);
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
    auto words = splitString(privateKey); // this is great to remove any type of whitespace
    if (words.size() != 1)
        throw std::runtime_error("Not valid private key");
    auto wallet = createWallet(walletName);
    wallet->setSingleAddressWallet(true);
    if (startHeight <= 1)
        startHeight = m_chain == P2PNet::MainChain ? 550000 : 1000;
    wallet->addPrivateKey(words.first().toString(), startHeight);
    emit walletsChanged();
    if (!m_offline)
        p2pNet()->addAction<SyncSPVAction>(); // make sure that we get peers for the new wallet.

    return new NewWalletConfig(wallet);
}

QObject *FloweePay::researchAddress(const QString &address, QObject *parent)
{
    CashAddress::Content c = CashAddress::decodeCashAddrContent(address.toStdString(), m_chainPrefix);
    if (c.hash.empty() || c.type != CashAddress::PUBKEY_TYPE) {
        logWarning() << "researchAddress() only works with a properly formatted cash-address!";
        return nullptr;
    }
    const KeyId key(reinterpret_cast<char *>(c.hash.data()));

    // if we don't know the address, return a nullptr
    AddressInfo *info = nullptr;

    for (const auto *wallet : std::as_const(m_wallets)) {
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

bool FloweePay::checkAppPassword(const QString &password)
{
    if (m_appProtection != AppPassword && m_appProtection != AppUnlocked)
        return false;

    // Possible attack vectors here are limited to the hash leaking to the
    // world and the users chosen password leaking.
    // As we aim for the hash to never leave the device any salting or similar
    // is irrelevant.
    const auto data = password.toUtf8();
    CSHA256 hasher;
    hasher.write(data.constData(), data.size());
    uint256 hash;
    hasher.finalize(hash.begin());
    const bool ok = (hash == m_appProtectionHash);
    if (ok) {
        setAppProtection(AppUnlocked);
        if (m_loadingCompleted && !m_loadCompletEmitted) {
            m_loadCompletEmitted = true;
            emit loadComplete();
        }
    }
    return ok;
}

void FloweePay::setAppPassword(const QString &password)
{
    if (password.isEmpty()) {
        m_appProtectionHash.SetNull();
        setAppProtection(NoProtection);
        return;
    }
    setAppProtection(AppUnlocked);
    const auto data = password.toUtf8();
    CSHA256 hasher;
    hasher.write(data.constData(), data.size());
    hasher.finalize(m_appProtectionHash.begin());
}

NewWalletConfig* FloweePay::createImportedHDWallet(const QString &mnemonic, const QString &password, const QString &derivationPathStr, const QString &walletName, const QDateTime &date, bool electrumFormat)
{
    const int height = p2pNet()->blockchain().blockHeightAtTime(date.toSecsSinceEpoch());
    return createImportedHDWallet(mnemonic, password, derivationPathStr, walletName, height, electrumFormat);
}

NewWalletConfig* FloweePay::createImportedHDWallet(const QString &mnemonic, const QString &password, const QString &derivationPathStr, const QString &walletName, int startHeight, bool electrumFormat)
{
    auto wallet = createWallet(walletName);
    try {
        std::vector<uint32_t> derivationPath = HDMasterKey::deriveFromString(derivationPathStr.toStdString());
        if (startHeight <= 1)
            startHeight = m_chain == P2PNet::MainChain ? 550000 : 1000;
        auto seedWords = splitString(mnemonic); // this is great to remove any type of whitespace
        wallet->createHDMasterKey(joinWords(seedWords, true), password.trimmed().remove('\n'),
                                  derivationPath, startHeight, electrumFormat);
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
    auto words = splitString(string);
    if (words.isEmpty()) {
        m_hdSeedValidator.clearSelectedLanguage();
        return WalletEnums::Unknown;
    }

    try {
        int firstWord = -2;
        // split into words.
        for (const auto word : words) {
            int index = m_hdSeedValidator.findWord(word.toString());
            if (firstWord == -2) {
                bool lowerCased = false;
                if (index == -1) {
                    // the first word, especially on Mobile, tends to start
                    // with an uppercase, expect that.
                    auto wordLc = word.toString().toLower();
                    if (wordLc != word) {
                        index = m_hdSeedValidator.findWord(wordLc);
                        lowerCased = true;
                    }
                }
                firstWord = index;
                if (index != -1) {
                    QString mnemonic = joinWords(words, lowerCased);
                    bool maybeElectrum;
                    auto validity = m_hdSeedValidator.validateMnemonic(mnemonic, index, &maybeElectrum);
                    if (validity == Mnemonic::Valid)
                        return WalletEnums::CorrectMnemonic;
                    else if (maybeElectrum)
                        return WalletEnums::ElectrumMnemonic;
                }
                else { // not a recognized word
                    break;
                }
            }
            else if (index == -1) { // a not-first-word failed the lookup.
                if (word != words.last()) // this is the last word, don't highlight while writing.
                    return WalletEnums::PartialMnemonicWithTypo;
                break;
            }
            // if we get to this point in the loop then we have a real word that we found in the dictionary.
            // Lets continue checking words and check if the rest of the words are part of the lexicon too.
        }
        if (firstWord >= 0)
            return WalletEnums::PartialMnemonic;
    } catch (const std::exception &e) {
        // probably deployment issues (faulty word list)
        logFatal() << e;
        return WalletEnums::MissingLexicon;
    }

    const QString string_ = joinWords(words, false);
    const std::string s = string_.toStdString();
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
    // start by validating user input
    std::vector<uint32_t> dp;
    try {
        // this throws should the path not validate.
        dp = HDMasterKey::deriveFromString(derivationPath.toStdString());
    } catch (const std::exception &e) {
        logFatal() << "Failed to parse user provided data due to:" << e;
        return nullptr;
    }

    // special case the first user-created wallet.
    // If the user creates a new wallet that is identical to the one we auto-created, reuse that one.
    const bool haveOneHiddenWallet = m_wallets.size() == 1 && !m_wallets.first()->userOwnedWallet();
    if (haveOneHiddenWallet) {
        auto wallet = m_wallets.first();
        if (wallet->isHDWallet() && derivationPath == wallet->derivationPath() && password == wallet->hdWalletMnemonicPwd()) {
            wallet->setUserOwnedWallet(true);
            if (!walletName.isEmpty())
                wallet->setName(walletName);
            assert(m_accountConfigs.contains(wallet->segment()->segmentId()));
            // little hacky to make listeners realize we really changed the wallet.
            m_wallets.clear();
            emit walletsChanged();
            m_wallets.append(wallet);
            emit walletsChanged();
            connectToWallet(wallet);
            return new NewWalletConfig(wallet);
        }
    }

    auto wallet = createWallet(walletName);
    std::vector<uint8_t> seed(16); // 12 word seed
    RandAddSeedPerfmon();
    GetRandBytes(seed.data(), seed.size());
    auto mnemonic = m_hdSeedValidator.generateMnemonic(seed, "en");
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

#ifdef TARGET_OS_Android
            // ask the Android system which interfaces there are;
            QJniEnvironment env;
            jclass floweeNetworks = env.findClass("org/flowee/pay/Networks");
            quint32 flags = QJniObject::callStaticMethod<jint>(floweeNetworks, "networkSupport", "()I");
            if (flags != 0) {
                logInfo() << "org.flowee.pay.Networks.networkSupport() returns flags:" << flags;
                auto &addressDb = p2pNet()->connectionManager().peerAddressDb();
                addressDb.setSupportIPv4Net((flags & 1) == 1);
                addressDb.setSupportIPv6Net((flags & 2) == 2);
            }
#endif
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
