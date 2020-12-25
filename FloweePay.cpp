/*
 * This file is part of the Flowee project
 * Copyright (C) 2020 Tom Zander <tomz@freedommail.ch>
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

#include <streaming/MessageParser.h>
#include <streaming/BufferPool.h>
#include <streaming/MessageBuilder.h>

#include <QStandardPaths>
#include <QCoreApplication>
#include <QLocale>
#include <QSettings>
#include <QFile>
#include <QFileInfo>
#include <base58.h>
#include <cashaddr.h>

constexpr const char *UNIT_TYPE = "general/unit";
constexpr const char *WINDOW_WIDTH = "window/width";
constexpr const char *WINDOW_HEIGHT = "window/height";
constexpr const char *DARKSKIN = "darkSkin";
constexpr const char *USERAGENT = "net/useragent";

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
    } else {
        m_chainPrefix = "bitcoincash";
    }
    boost::filesystem::create_directories(boost::filesystem::path(m_basedir.toStdString()));

    // make it move to the proper thread.
    connect(this, SIGNAL(loadComplete_priv()), SIGNAL(loadComplete()), Qt::QueuedConnection);

    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, [=]() {
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

    QSettings appConfig;
    m_unit = static_cast<UnitOfBitcoin>(appConfig.value(UNIT_TYPE, m_unit).toInt());
    m_windowHeight = appConfig.value(WINDOW_HEIGHT, m_windowHeight).toInt();
    m_windowWidth = appConfig.value(WINDOW_WIDTH, m_windowWidth).toInt();
    m_darkSkin = appConfig.value(DARKSKIN, m_darkSkin).toBool();
}

FloweePay::~FloweePay()
{
    saveData();

    qDeleteAll(m_wallets);
    QSettings appConfig;
    appConfig.setValue(WINDOW_HEIGHT, m_windowHeight);
    appConfig.setValue(WINDOW_WIDTH, m_windowWidth);
    appConfig.setValue(DARKSKIN, m_darkSkin);
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

    QFile in(m_basedir + "/appdata");
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

    if (m_wallets.isEmpty())
        createNewWallet();
    emit loadComplete_priv();
}

void FloweePay::saveData()
{
    Streaming::BufferPool data;
    Streaming::MessageBuilder builder(data);
    for (auto &wallet : m_wallets) {
        builder.add(WalletId, wallet->segment()->segmentId());
        builder.add(WalletPriority, wallet->segment()->priority());
    }
    QString filebase = m_basedir + "/appdata";
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

QString FloweePay::priceToString(qint64 price) const
{
    if (m_unit == Satoshis)
        return QString::number(price);
    QByteArray string(QByteArray::number(std::abs(price)));

    int decimals;
    switch (m_unit) {
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

    saveData();
    emit walletsChanged();
    return w;
}

P2PNet::Chain FloweePay::chain() const
{
    return m_chain;
}

void FloweePay::setChain(const P2PNet::Chain &chain)
{
    m_chain = chain;
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
    emit unitChanged();
}

int FloweePay::headerChainHeight() const
{
    if (!m_downloadManager.get())
        return 0;
    return m_downloadManager->blockHeight();
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
    emit windowHeightChanged();
}

int FloweePay::windowWidth() const
{
    return m_windowWidth;
    // QSettings appSettings;
    // return appSettings.value(WINDOW_WIDTH, 800).toInt();
}

void FloweePay::setWindowWidth(int windowWidth)
{
    if (windowWidth == m_windowWidth)
        return;
    m_windowWidth = windowWidth;
    emit windowWidthChanged();
    // QSettings appSettings;
    // appSettings.setValue(WINDOW_WIDTH, windowWidth);
}

int FloweePay::walletStartHeightHint() const
{
    static int minStartBlock = [] {
        switch (s_chain) {
        case P2PNet::MainChain: return 636000;
        case P2PNet::Testnet4Chain: return 13000;
        default: return 0;
        }
    }();
    return std::max(minStartBlock, m_downloadManager->blockHeight());
}

void FloweePay::createImportedWallet(const QString &privateKey, const QString &walletName)
{
    auto wallet = createWallet(walletName);
    // TODO add a lookup to an indexer to tell us the oldest use of the resulting address
    wallet->addPrivateKey(privateKey, walletStartHeightHint());
}

FloweePay::StringType FloweePay::identifyString(const QString &string) const
{
    std::string s = string.trimmed().toStdString();

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

    return Unknown;
}

void FloweePay::createNewWallet(const QString &walletName)
{
    auto wallet = createWallet(walletName);
    wallet->createNewPrivateKey(walletStartHeightHint());
}

QString FloweePay::unitName() const
{
    switch (m_unit) {
    case FloweePay::BCH:
        return QLatin1String("BCH");
    case FloweePay::MilliBCH:
        return QLatin1String("mBCH");
    case FloweePay::MicroBCH:
        return QString("µBCH");
    case FloweePay::Bits:
        return QLatin1String("bits");
    case FloweePay::Satoshis:
        return QLatin1String("sats");
    default:
        Q_ASSERT(false);
        return 0;
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

        QSettings defaultConfig(":/defaults.ini", QSettings::IniFormat);
        QString useragent = defaultConfig.value(USERAGENT, "Flowee Pay Wallet").toString();
        m_downloadManager->connectionManager().setUserAgent(useragent.toStdString());
        emit headerChainHeightChanged();
    }
    return m_downloadManager.get();
}

// static
Streaming::BufferPool &FloweePay::pool(int reserveSize)
{
    return FloweePay::instance()->p2pNet()->connectionManager().pool(reserveSize);
}

const std::string &chainPrefix()
{
    return FloweePay::instance()->chainPrefix();
}
