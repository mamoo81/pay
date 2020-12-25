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
#include "NetDataProvider.h"
#include "PortfolioDataProvider.h"
#include "FloweePay.h"
#include "BitcoinValue.h"

#include <primitives/key.h> // for ECC_Start()

#include <QCommandLineParser>
#include <QDateTime>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QStandardPaths>

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
    // qputenv("QT_IM_MODULE", QByteArray("qtvirtualkeyboard"));
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QGuiApplication qapp(argc, argv);
    qapp.setOrganizationName("flowee");
    qapp.setApplicationName("pay");
    qapp.setApplicationVersion("0.3");

    srand((quint32) QDateTime::currentMSecsSinceEpoch());

    qmlRegisterType<FloweePay>("Flowee.org.pay", 1, 0, "Pay");
    qmlRegisterType<BitcoinValue>("Flowee.org.pay", 1, 0, "BitcoinValue");
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

    // select chain
    if (parser.isSet(testnet4))
        FloweePay::selectChain(P2PNet::Testnet4Chain);

    ECC_State crypo_state; // allows the secp256k1 to function.
    qmlRegisterType<TransactionInfo>("org.flowee", 1, 0, "TransactionInfo");
    QQmlApplicationEngine engine;
    const QUrl url(QStringLiteral("qrc:/main.qml"));
    engine.rootContext()->setContextProperty("Flowee", FloweePay::instance());
    engine.load(url);

    QObject::connect(FloweePay::instance(), &FloweePay::loadComplete, [&engine, &parser, &connect]() {
        FloweePay *app = FloweePay::instance();

        NetDataProvider *netData = new NetDataProvider(app->p2pNet()->blockHeight(), &engine);
        app->p2pNet()->addP2PNetListener(netData);

        PortfolioDataProvider *portfolio = new PortfolioDataProvider(&engine);
        for (const auto &wallet : app->wallets()) {
            portfolio->addWalletAccount(wallet);
        }
        portfolio->selectDefaultWallet();

        engine.rootContext()->setContextProperty("net", netData);
        engine.rootContext()->setContextProperty("portfolio", portfolio);
        if (parser.isSet(connect)) {
            app->p2pNet()->connectionManager().peerAddressDb().addOne( // actually connect to it too.
                        EndPoint(parser.value(connect).toStdString(), 8333));
        }
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
