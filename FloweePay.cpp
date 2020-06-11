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

#define FONTSIZE "FONT_SIZE"
#define UNIT_TYPE "UNIT_TYPE"
#define WINDOW_WIDTH "windowWidth"
#define WINDOW_HEIGHT "windowHeight"

enum FileTags {
    WalletId
};

FloweePay::FloweePay()
    : m_basedir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
{
    // make it move to the proper thread.
    connect(this, SIGNAL(loadComplete_priv()), SIGNAL(loadComplete()), Qt::QueuedConnection);

    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, [=]() {
        p2pNet()->shutdown();
        saveAll();
    });

    // start creation of downloadmanager and loading of data in a different thread
    ioService().post(std::bind(&FloweePay::init, this));

    QSettings appConfig;
    m_unit = static_cast<UnitOfBitcoin>(appConfig.value(UNIT_TYPE, BCH).toInt());
}

FloweePay::~FloweePay()
{
    qDeleteAll(m_wallets);
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
    if (in.open(QIODevice::ReadOnly)) {
        const auto dataSize = in.size();
        Streaming::BufferPool pool(dataSize);
        in.read(pool.begin(), dataSize);
        Streaming::MessageParser parser(pool.commit(dataSize));
        while (parser.next() == Streaming::FoundTag) {
            if (parser.tag() == WalletId) {
                Wallet *w = new Wallet(m_basedir.toStdString(), parser.intData());
                dl->addDataListener(w);
                dl->connectionManager().addPrivacySegment(w->segment());
                m_wallets.append(w);
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
    for (auto wallet : m_wallets) {
        builder.add(WalletId, wallet->segment()->segmentId());
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
    for (auto wallet : m_wallets) {
        wallet->saveWallet();
    }
}

QString FloweePay::basedir() const
{
    return m_basedir;
}

QString FloweePay::priceToString(qint64 price) const
{
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
        if (!QFileInfo(m_basedir + dir).exists())
            break;
    }

    Wallet *w = new Wallet(m_basedir.toStdString(), id);
    if (!name.isEmpty())
        w->setName(name);
    dl->addDataListener(w);
    dl->connectionManager().addPrivacySegment(w->segment());
    m_wallets.append(w);

    saveData();
    emit walletsChanged();
    return w;
}

int FloweePay::windowHeight() const
{
    QSettings appSettings;
    return appSettings.value(WINDOW_HEIGHT, 600).toInt();
}

void FloweePay::setWindowHeight(int windowHeight)
{
    QSettings appSettings;
    appSettings.setValue(WINDOW_HEIGHT, windowHeight);
}

int FloweePay::windowWidth() const
{
    QSettings appSettings;
    return appSettings.value(WINDOW_WIDTH, 800).toInt();
}

void FloweePay::setWindowWidth(int windowWidth)
{
    QSettings appSettings;
    appSettings.setValue(WINDOW_WIDTH, windowWidth);
}

void FloweePay::createImportedWallet(const QString &privateKey, const QString &walletName)
{
    auto wallet = createWallet(walletName);
    // TODO add a lookup to an indexer to tell us the oldest use of the resulting address
    wallet->addPrivateKey(privateKey, 500000);
}

FloweePay::StringType FloweePay::identifyString(const QString &string) const
{
    std::string s = string.toStdString();

    CBase58Data legacy;
    if (legacy.SetString(s)) {
        if (legacy.isMainnetSh())
            return LegacySH;
        if (legacy.isMainnetPkh())
            return LegacyPKH;
        if (legacy.isMainnetPrivKey())
            return PrivateKey;
    }

    CashAddress::Content c = CashAddress::decodeCashAddrContent(s, "bitcoincash");
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
    wallet->createNewPrivateKey(std::max(636000, m_downloadManager->blockHeight()));
}

QString FloweePay::unitName() const
{
    return QString("BCH");
}

QList<Wallet *> FloweePay::wallets() const
{
    return m_wallets;
}

DownloadManager *FloweePay::p2pNet()
{
    if (m_downloadManager == nullptr)
        m_downloadManager.reset(new DownloadManager(ioService(), m_basedir.toStdString()));
    return m_downloadManager.get();
}
