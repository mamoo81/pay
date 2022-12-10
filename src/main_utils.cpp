/*
 * This file is part of the Flowee project
 * Copyright (C) 2022 Tom Zander <tom@flowee.org>
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
#include "qqmlcontext.h"
#include <P2PNet.h>
#include <FloweePay.h>

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QCommandLineParser>
#include <QStringList>
#include <QFileInfo>
#include <NetDataProvider.h>
#include <PortfolioDataProvider.h>

struct CommandLineParserData
{
    CommandLineParserData(QGuiApplication &qapp)
        : connect(QStringList() << "connect", "Connect to HOST", "HOST"),
        debug(QStringList() << "debug", "Use debug level logging"),
        verbose(QStringList() << "verbose" << "v", "Be more verbose"),
        quiet(QStringList() << "quiet" << "q", "Be quiet, only errors are shown"),
        testnet4(QStringList() << "testnet4", "Use testnet4"),
        offline(QStringList() << "offline", "Do not connect"),
        headers(QStringList() << "headers", "Override location of blockheaders", "PATH")
    {
        parser.setApplicationDescription("Flowee pay");
        parser.addHelpOption();
        parser.addOption(debug);
        parser.addOption(verbose);
        parser.addOption(quiet);
        parser.addOption(connect);
        parser.addOption(testnet4);
        parser.addOption(offline);
#ifndef NDEBUG
        // to protect people from the bad effect of having and later not having headers we only allow this
        // override in debug mode.
        parser.addOption(headers);
#endif
        parser.process(qapp);

        // select chain first (before we create the FloweePay singleton)
        if (parser.isSet(testnet4))
            chain = P2PNet::Testnet4Chain;
    }

    QCommandLineParser parser;
    QCommandLineOption connect;
    QCommandLineOption debug;
    QCommandLineOption verbose;
    QCommandLineOption quiet;
    QCommandLineOption testnet4;
    QCommandLineOption offline;
    QCommandLineOption headers;
    P2PNet::Chain chain = P2PNet::MainChain;
};

CommandLineParserData* createCLD(QGuiApplication &app)
{
    return new CommandLineParserData(app);
}

Log::Verbosity logVerbosity(CommandLineParserData *cld)
{
#ifndef BCH_NO_DEBUG_OUTPUT
    if (cld->parser.isSet(cld->debug))
        return Log::DebugLevel;
#endif
    if (cld->parser.isSet(cld->verbose))
        return Log::InfoLevel;
    if (cld->parser.isSet(cld->quiet))
        return Log::FatalLevel;
    return Log::WarningLevel;
}

std::unique_ptr<QFile> handleStaticChain(CommandLineParserData *cld)
{
    std::unique_ptr<QFile> blockheaders; // pointer to own the memmapped blockheaders file.


    blockheaders.reset(new QFile(QString("/usr/share/floweepay/")
                           + (cld->chain == P2PNet::MainChain ? "blockheaders" : "blockheaders-testnet4")));
#ifndef NDEBUG
    // override only available in debug mode
    if (cld->parser.isSet(cld->headers)) {
        QFileInfo info(cld->parser.value(cld->headers));
        if (info.exists()) {
            if (info.isDir())
                blockheaders.reset(new QFile(info.absoluteFilePath()
                                             + (cld->chain == P2PNet::MainChain ? "/blockheaders" : "/blockheaders-testnet4")));
            else
                blockheaders.reset(new QFile(info.absoluteFilePath()));
        }
        else {
            // do not load if pointing to invalid path.
            logWarning() << "Headers disabled by cli option";
            blockheaders.reset();
        }
    }
#endif

    if (blockheaders) {
        if (!blockheaders->open(QIODevice::ReadOnly)) { // can't be opened for reading.
            blockheaders.reset();
        }
        else {
            QString infoFilePath = blockheaders->fileName() + ".info";
            Blockchain::setStaticChain(blockheaders->map(0, blockheaders->size()), blockheaders->size(),
                                      infoFilePath.toStdString());
            blockheaders->close();
        }
    }
    return blockheaders;
}

void loadCompleteHandler(QQmlApplicationEngine &engine, CommandLineParserData *cld)
{
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
    engine.rootContext()->setContextProperty("fiatHistory", FloweePay::instance()->priceHistory());
    engine.rootContext()->setContextProperty("portfolio", portfolio);
    if (cld->parser.isSet(cld->offline)) {
        FloweePay::instance()->setOffline(true);
    }
    else if (cld->parser.isSet(cld->connect)) {
        app->p2pNet()->connectionManager().peerAddressDb().addOne( // add it to the DB, making sure there is at least one.
                    EndPoint(cld->parser.value(cld->connect).toStdString(), 8333));
    }
    app->startNet(); // lets go!
}
