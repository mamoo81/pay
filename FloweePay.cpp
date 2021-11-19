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
#include "FloweePay.h"
#include "Wallet.h"
#include "NewWalletConfig.h"

#include <streaming/MessageParser.h>
#include <streaming/BufferPool.h>
#include <streaming/MessageBuilder.h>
#include <random.h>
#include <config/flowee-config.h>

#include <QStandardPaths>
#include <QGuiApplication>
#include <QClipboard>
#include <QLocale>
#include <QSettings>
#include <QFile>
#include <QFileInfo>
#include <base58.h>
#include <cashaddr.h>
#include <QTimer>
#include <QResource>
#include <QDir>
#include <SyncSPVAction.h>

constexpr const char *UNIT_TYPE = "unit";
constexpr const char *CREATE_START_WALLET = "create-start-wallet";
constexpr const char *WINDOW_WIDTH = "window/width";
constexpr const char *WINDOW_HEIGHT = "window/height";
constexpr const char *DARKSKIN = "darkSkin";
constexpr const char *HIDEBALANCE = "hideBalance";
constexpr const char *USERAGENT = "net/useragent";
constexpr const char *DSPTIMEOUT = "payment/dsp-timeout";

constexpr const char *AppdataFilename = "/appdata";
constexpr const char *DefaultDerivationPath = "m/44'/145'/0'";
constexpr const char *DefaultDerivationPathTestnet = "m/44'/145'/0'";

enum FileTags {
    WalletId,
    WalletPriority   // int, maps to PrivacySegment::Priority
};

static P2PNet::Chain s_chain = P2PNet::MainChain;

FloweePay::FloweePay()
    : m_basedir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)),
    m_chain(s_chain)
{
    if (m_chain == P2PNet::Testnet4Chain) {
        m_basedir += "/testnet4";
        m_chainPrefix = "bchtest";
        m_defaultDerivationPath = QLatin1String(DefaultDerivationPathTestnet);
    } else {
        m_chainPrefix = "bitcoincash";
        m_defaultDerivationPath = QLatin1String(DefaultDerivationPath);
    }
    boost::filesystem::create_directories(boost::filesystem::path(m_basedir.toStdString()));

    // make it move to the proper thread.
    connect(this, SIGNAL(loadComplete_priv()), this, SLOT(loadingCompleted()), Qt::QueuedConnection);
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, [=]() {
        p2pNet()->shutdown();
        saveAll();
    });

    // start creation of downloadmanager and loading of data in a different thread
    ioService().post(std::bind(&FloweePay::init, this));

    QSettings defaultConfig(":/defaults.ini", QSettings::IniFormat);
    m_unit = static_cast<UnitOfBitcoin>(defaultConfig.value(UNIT_TYPE, BCH).toInt());
    m_windowHeight = defaultConfig.value(WINDOW_HEIGHT, -1).toInt();
    m_windowWidth = defaultConfig.value(WINDOW_WIDTH, -1).toInt();
    m_darkSkin = defaultConfig.value(DARKSKIN, true).toBool();
    m_dspTimeout = defaultConfig.value(DSPTIMEOUT, 3000).toInt();
    m_createStartWallet = defaultConfig.value(CREATE_START_WALLET, false).toBool();

    QSettings appConfig;
    m_unit = static_cast<UnitOfBitcoin>(appConfig.value(UNIT_TYPE, m_unit).toInt());
    m_windowHeight = appConfig.value(WINDOW_HEIGHT, m_windowHeight).toInt();
    m_windowWidth = appConfig.value(WINDOW_WIDTH, m_windowWidth).toInt();
    m_darkSkin = appConfig.value(DARKSKIN, m_darkSkin).toBool();
    m_dspTimeout = appConfig.value(DSPTIMEOUT, m_dspTimeout).toInt();
    m_hideBalance = appConfig.value(HIDEBALANCE, false).toBool();

    // Update expected chain-height ever 5 minutes
    QTimer *timer = new QTimer(this);
    timer->setTimerType(Qt::VeryCoarseTimer);
    timer->start(5 * 60 * 1000);
    connect (timer, SIGNAL(timeout()), this, SIGNAL(expectedChainHeightChanged()));

    QDir base(QCoreApplication::applicationDirPath() + "/../share/floweepay/");
    if (base.exists()) {
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
            QString fullPath(base.absoluteFilePath(lang.filename));
            if (QFile::exists(fullPath))
                m_hdSeedValidator.registerWordList(lang.id, fullPath);
        }
    }
    else {
        logCritical() << "Warning: No bip39 wordlists found. Looking in:" << base.absolutePath();
    }
}

FloweePay::~FloweePay()
{
    saveData();

    qDeleteAll(m_wallets);
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
        while (parser.next() == Streaming::FoundTag) {
            if (parser.tag() == WalletId) {
                try {
                    Wallet *w = new Wallet(m_basedir.toStdString(), parser.intData());
                    w->moveToThread(thread());
                    dl->addDataListener(w);
                    dl->connectionManager().addPrivacySegment(w->segment());
                    m_wallets.append(w);
                    lastOpened = w;
                } catch (const std::runtime_error &e) {
                    logWarning() << "Wallet load failed:" << e;
                    lastOpened = nullptr;
                }
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
        }
    }

    if (m_wallets.isEmpty() && m_createStartWallet) {
        createNewWallet(m_defaultDerivationPath);
        m_wallets.at(0)->setUserOwnedWallet(false);
        m_wallets.at(0)->segment()->setPriority(PrivacySegment::Last);
        saveData();
    }
    emit loadComplete_priv(); // move execution to loadingCompleted, in a Qt thread
}

void FloweePay::loadingCompleted()
{
    for (auto wallet : m_wallets) {
        wallet->performUpgrades();
    }

    emit loadComplete();
}

void FloweePay::saveData()
{
    Streaming::BufferPool data;
    Streaming::MessageBuilder builder(data);
    for (auto &wallet : m_wallets) {
        builder.add(WalletId, wallet->segment()->segmentId());
        builder.add(WalletPriority, wallet->segment()->priority());
    }
    QString filebase = m_basedir + AppdataFilename;
    QFile out(filebase + "~");
    out.remove(); // avoid overwrite issues.
    if (out.open(QIODevice::WriteOnly)) {
        auto buf = builder.buffer();
        auto rc = out.write(buf.begin(), buf.size());
        if (rc == -1) {
            logFatal() << "Failed to write. Disk full?";
            // TODO have an app-wide error
            return;
        }
        out.close();
        QFile::remove(filebase);
        if (!out.rename(filebase)) {
            logFatal() << "Failed to write to" << filebase;
        // TODO have an app-wide error
        }
    } else {
        logFatal() << "Failed to create data file. Disk full?";
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

// static
QString FloweePay::priceToString(qint64 price, UnitOfBitcoin unit)
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
    const char decimalPoint = QLocale::system().decimalPoint().unicode();

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
    return date.toString(format);
}

QString FloweePay::formatDateTime(QDateTime date) const
{
    static QString format = QLocale::system().dateTimeFormat(QLocale::ShortFormat);
    static QString timeFormat = QLocale::system().timeFormat(QLocale::ShortFormat);
    if (!date.isValid() || date.isNull())
        return QString();

    const QDateTime now = QDateTime::currentDateTime();
    if (now > date) {
        // use the 'yesterday' style if the date is reasonably close.
        const auto secs = date.secsTo(now);
        if (secs < 24 * 60)
            return tr("Now", "timestamp");
        if (secs < 66 * 60)
            return tr("An hour ago", "timestamp");
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
        id = rand();
        QString dir = QString("/wallet-%1").arg(id);
        if (!QFileInfo::exists(m_basedir + dir))
            break;
    }

    Wallet *w = Wallet::createWallet(m_basedir.toStdString(), id, name);
    dl->addDataListener(w);
    dl->connectionManager().addPrivacySegment(w->segment());
    m_wallets.append(w);

    emit walletsChanged();
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
    if (m_initialHeaderChainHeight <= 0)
        m_initialHeaderChainHeight = headerChainHeight();

    const int hch = headerChainHeight();
    if (m_initialHeaderChainHeight == headerChainHeight()) {
        const int expected = expectedChainHeight();
        const int behind = expected - hch; // num blocks we are behind theoretical height
        if (behind > 3) // don't report expected when variance could explain the diff
            return expected;
    }
    return headerChainHeight();
}

void FloweePay::blockchainHeightChanged(int)
{
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

bool FloweePay::preferSchnorr() const
{
    return m_preferSchnorr;
}

void FloweePay::setPreferSchnorr(bool preferSchnorr)
{
    if (m_preferSchnorr == preferSchnorr)
        return;

    m_preferSchnorr = preferSchnorr;
    emit preferSchnorrChanged();
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

NewWalletConfig* FloweePay::createImportedWallet(const QString &privateKey, const QString &walletName, int startHeight)
{
    auto wallet = createWallet(walletName);
    wallet->setSingleAddressWallet(true);
    if (startHeight <= 1)
        startHeight = s_chain == P2PNet::MainChain ? 550000 : 1000;
    wallet->addPrivateKey(privateKey, startHeight);
    saveData();
    p2pNet()->addAction<SyncSPVAction>(); // make sure that we get peers for the new wallet.

    return new NewWalletConfig(wallet);
}

NewWalletConfig* FloweePay::createImportedHDWallet(const QString &mnemonic, const QString &password, const QString &derivationPathStr, const QString &walletName, int startHeight)
{
    auto wallet = createWallet(walletName);
    try {
        std::vector<uint32_t> derivationPath = HDMasterKey::deriveFromString(derivationPathStr.toStdString());
        if (startHeight <= 1)
            startHeight = s_chain == P2PNet::MainChain ? 550000 : 1000;
        wallet->createHDMasterKey(mnemonic, password, derivationPath, startHeight);
        wallet->segment()->blockSynched(startHeight);
        wallet->segment()->blockSynched(startHeight); // yes, twice
        saveData();
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

FloweePay::StringType FloweePay::identifyString(const QString &string) const
{
    const QString string_ = string.trimmed();
    const std::string s = string_.toStdString();
    if (string_.isEmpty()) {
        m_hdSeedValidator.clearSelectedLanguage();
        return Unknown;
    }

    CBase58Data legacy;
    if (legacy.SetString(s)) {
        if ((m_chain == P2PNet::MainChain && legacy.isMainnetPkh())
            || (m_chain == P2PNet::Testnet4Chain && legacy.isTestnetPkh()))
            return LegacyPKH;
        if ((m_chain == P2PNet::MainChain && legacy.isMainnetSh())
            || (m_chain == P2PNet::Testnet4Chain && legacy.isTestnetSh()))
            return LegacySH;
        if ((m_chain == P2PNet::MainChain && legacy.isMainnetPrivKey())
            || (m_chain == P2PNet::Testnet4Chain && legacy.isTestnetPrivKey()))
            return PrivateKey;
    }

    CashAddress::Content c = CashAddress::decodeCashAddrContent(s, m_chainPrefix);
    if (!c.hash.empty()) {
        if (c.type == CashAddress::PUBKEY_TYPE)
            return CashPKH;
        if (c.type == CashAddress::SCRIPT_TYPE)
            return CashSH;
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
                            return CorrectMnemonic;
                    }
                    else { // not a recognized word
                        break;
                    }
                }
                else if (index == -1) { // a not-first-word failed the lookup.
                    if (space2 != -1) // this is the last word, don't highlight while writing.
                        return PartialMnemonicWithTypo;
                    break;
                }
                // if we get to this point in the loop then we have a real word that we found in the dictionary.
                // Lets continue checking words and check if the rest of the words are part of the lexicon too.
            }
            space = space2;
        } while (space != -1);
        if (firstWord >= 0)
            return PartialMnemonic;
    } catch (const std::exception &e) {
        // probably deployment issues (faulty word list)
        logFatal() << e;
        return MissingLexicon;
    }
    return Unknown;
}

NewWalletConfig* FloweePay::createNewBasicWallet(const QString &walletName)
{
    auto wallet = createWallet(walletName);
    wallet->createNewPrivateKey(walletStartHeightHint());
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
        if (s_chain == P2PNet::MainChain)
            return QLatin1String("BCH");
        else
            return QLatin1String("tBCH");
    case FloweePay::MilliBCH:
        if (s_chain == P2PNet::MainChain)
            return QLatin1String("mBCH");
        else
            return QLatin1String("m-tBCH");
    case FloweePay::MicroBCH:
        if (s_chain == P2PNet::MainChain)
            return QString("µBCH");
        else
            return QString("µ-tBCH");
    case FloweePay::Bits:
        if (s_chain == P2PNet::MainChain)
            return QLatin1String("bits");
        else
            return QLatin1String("tbits");
    case FloweePay::Satoshis:
        if (s_chain == P2PNet::MainChain)
            return QLatin1String("sats");
        else
            return QLatin1String("tsats");
    default:
        return QString();
    }
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
        m_downloadManager->addP2PNetListener(this);
        m_downloadManager->notifications().addListener(&m_notifications);

        QSettings defaultConfig(":/defaults.ini", QSettings::IniFormat);
        QString useragent = defaultConfig.value(USERAGENT, "Flowee Pay Wallet").toString();
        m_downloadManager->connectionManager().setUserAgent(useragent.toStdString());
        emit headerChainHeightChanged();
        emit expectedChainHeightChanged();
    }
    return m_downloadManager.get();
}

// static
Streaming::BufferPool &FloweePay::pool(int reserveSize)
{
    return FloweePay::instance()->p2pNet()->connectionManager().pool(reserveSize);
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
