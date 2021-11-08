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
#include "BitcoinValue.h"
#include "FloweePay.h"
#include "NetDataProvider.h"
#include "NewWalletConfig.h"
#include "PortfolioDataProvider.h"
#include "PriceDataProvider.h"
#include "QRCreator.h"

#include <primitives/key.h> // for ECC_Start()

#include <QCommandLineParser>
#include <QDateTime>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QStandardPaths>
#include <QTranslator>

#include <signal.h>

namespace {
void HandleSigTerm(int) {
    QCoreApplication::quit();
}

struct ECC_State
{
    ECC_State() {
        // Init crypto lib.
        ECC_Start();
    }
    ~ECC_State() {
        ECC_Stop();
    }
};
}

int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QGuiApplication qapp(argc, argv);
    qapp.setOrganizationName("flowee");
    qapp.setApplicationName("pay");
    qapp.setApplicationVersion("2021.05");
    qapp.setWindowIcon(QIcon(":/FloweePay.png"));

    srand((quint32) QDateTime::currentMSecsSinceEpoch());

    qmlRegisterType<FloweePay>("Flowee.org.pay", 1, 0, "Bitcoin");
    qmlRegisterType<BitcoinValue>("Flowee.org.pay", 1, 0, "BitcoinValue");
    qmlRegisterType<NewWalletConfig>("Flowee.org.pay", 1, 0, "NewWalletConfig");
    QCommandLineParser parser;
    parser.setApplicationDescription("Flowee pay");
    QCommandLineOption debug(QStringList() << "debug", "Use debug level logging");
    QCommandLineOption verbose(QStringList() << "verbose" << "v", "Be more verbose");
    QCommandLineOption quiet(QStringList() << "quiet" << "q", "Be quiet, only errors are shown");
    QCommandLineOption connect(QStringList() << "connect", "Connect to HOST", "HOST");
    QCommandLineOption testnet4(QStringList() << "testnet4", "Use testnet4");
    parser.addHelpOption();
    parser.addOption(debug);
    parser.addOption(verbose);
    parser.addOption(quiet);
    parser.addOption(connect);
    parser.addOption(testnet4);
#ifndef NDEBUG
    // to protect people from the bad effect of having and later not having headers we only allow this
    // override in debug mode.
    QCommandLineOption headers(QStringList() << "headers", "Override location of blockheaders", "PATH");
    parser.addOption(headers);
    // nice feature to test your QML changes.
    QCommandLineOption offline(QStringList() << "offline", "Do not connect");
    parser.addOption(offline);
#endif
    parser.process(qapp);

    auto *logger = Log::Manager::instance();
    logger->clearChannels();
    Log::Verbosity v = Log::WarningLevel;
#ifndef BCH_NO_DEBUG_OUTPUT
    if (parser.isSet(debug))
        v = Log::DebugLevel;
    else
#endif
    if (parser.isSet(verbose))
        v = Log::InfoLevel;
    else if (parser.isSet(quiet))
        v = Log::FatalLevel;
    logger->clearLogLevels(v);
    logger->addConsoleChannel();

    QTranslator translator;
    if (translator.load(QLocale(), QLatin1String("floweepay"), QLatin1String("_"), QLatin1String(":/i18n")))
        QCoreApplication::installTranslator(&translator);

    PriceDataProvider prices;
    // select chain
    auto chain = P2PNet::MainChain;
    if (parser.isSet(testnet4))
        chain = P2PNet::Testnet4Chain;
    else
        prices.start();
    FloweePay::selectChain(chain);

    bool online = true;
    std::unique_ptr<QFile> blockheaders; // pointer to own the memmapped blockheaders file.
    // lets try by default to open the path /usr/share/floweepay/*
    blockheaders.reset(new QFile(QString("/usr/share/floweepay/")
                                 + (chain == P2PNet::MainChain ? "blockheaders" : "blockheaders-testnet4")));
#ifndef NDEBUG
    // override only available in debug mode
    if (parser.isSet(headers)) {
        QFileInfo info(parser.value(headers));
        if (info.exists()) {
            if (info.isDir())
                blockheaders.reset(new QFile(info.absoluteFilePath()
                                             + (chain == P2PNet::MainChain ? "/blockheaders" : "/blockheaders-testnet4")));
            else
                blockheaders.reset(new QFile(info.absoluteFilePath()));
        }
        else {
            // do not load if pointing to invalid path.
            logWarning() << "Headers disabled by cli option";
            blockheaders.reset();
        }
    }
    online = !parser.isSet(offline);

#endif
    if (blockheaders) {
       if (!blockheaders->open(QIODevice::ReadOnly)) { // can't be opened for reading.
            blockheaders.reset();
       }
       else {
           Blockchain::setStaticChain(blockheaders->map(0, blockheaders->size()), blockheaders->size());
           blockheaders->close();
       }
    }

    ECC_State crypo_state; // allows the secp256k1 to function.
    qmlRegisterType<TransactionInfo>("Flowee.org.pay", 1, 0, "TransactionInfo");
    qmlRegisterType<PaymentRequest>("Flowee.org.pay", 1, 0, "PaymentRequest");
    QQmlApplicationEngine engine;
    engine.addImageProvider(QLatin1String("qr"), new QRCreator());

    const QUrl url(QStringLiteral("qrc:/main.qml"));
    engine.rootContext()->setContextProperty("Pay", FloweePay::instance());
    engine.rootContext()->setContextProperty("Fiat", &prices);
    engine.load(url);

    QObject::connect(FloweePay::instance(), &FloweePay::loadComplete, &engine, [&engine, &parser, &connect, online]() {
        FloweePay *app = FloweePay::instance();

        NetDataProvider *netData = new NetDataProvider(app->p2pNet()->blockHeight(), &engine);
        app->p2pNet()->addP2PNetListener(netData);
        netData->startRefreshTimer();

        PortfolioDataProvider *portfolio = new PortfolioDataProvider(&engine);
        for (auto &wallet : app->wallets()) {
            portfolio->addWalletAccount(wallet);
        }
        portfolio->selectDefaultWallet();

        engine.rootContext()->setContextProperty("net", netData);
        engine.rootContext()->setContextProperty("portfolio", portfolio);
        if (parser.isSet(connect)) {
            app->p2pNet()->connectionManager().peerAddressDb().addOne( // actually connect to it too.
                        EndPoint(parser.value(connect).toStdString(), 8333));
        }
        if (online) // see the 'offline' commandline option (debug builds only)
            app->p2pNet()->start(); // lets go!
    });

    // Clean shutdown on SIGTERM
    struct sigaction sa;
    sa.sa_handler = HandleSigTerm;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGINT, &sa, nullptr);
    // Ignore SIGPIPE
    signal(SIGPIPE, SIG_IGN);

    return qapp.exec();
}
