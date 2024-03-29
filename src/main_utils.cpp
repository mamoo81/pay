/*
 * This file is part of the Flowee project
 * Copyright (C) 2022-2024 Tom Zander <tom@flowee.org>
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
    explicit CommandLineParserData(QGuiApplication &qapp)
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
        parser.addPositionalArgument("pay-data", "Payment URI", "[bitcoincash:/xxx]");
#ifndef NDEBUG
        // to protect people from the bad effect of having and later not having headers we only allow this
        // override in debug mode.
        parser.addOption(headers);
#endif
        parser.process(qapp);

        // select chain first (before we create the FloweePay singleton)
        if (parser.isSet(testnet4))
            chain = P2PNet::Testnet4Chain;
        FloweePay::selectChain(chain);
        if (parser.isSet(offline))
            FloweePay::instance()->setOffline(true);
        payRequest = parser.positionalArguments();
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

    QStringList payRequest;
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
            bool needsCreation = false;
            if (!QFile::exists(infoFilePath)) {
                // early versions of Flowee Pay shipped the info file together with the
                // static file in the original install, lets check if we should instead
                // create one here on first run.
                infoFilePath = FloweePay::instance()->basedir() + "/staticHeaders.info";
                QFileInfo meta(infoFilePath);
                QFileInfo data(blockheaders->fileName());
                if (!meta.exists())
                    needsCreation = true;
                else if (data.lastModified() > meta.lastModified())
                    needsCreation = true;
            }
            if (needsCreation) // create a user-local info file.
                Blockchain::createStaticHeaders(blockheaders->fileName().toStdString(),
                                                infoFilePath.toStdString());
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
    if (cld->payRequest.size() == 1)
        app->setPaymentProtocolRequest(cld->payRequest.first());

    NetDataProvider *netData = new NetDataProvider(&engine);
    app->p2pNet()->addP2PNetListener(netData);

    const bool isOffline = cld->parser.isSet(cld->offline);
    if (!isOffline)
        netData->startTimer();

    PortfolioDataProvider *portfolio = new PortfolioDataProvider(&engine);
    engine.rootContext()->setContextProperty("net", netData);
    engine.rootContext()->setContextProperty("portfolio", portfolio);
    if (!isOffline && cld->parser.isSet(cld->connect)) {
        const int port = cld->parser.isSet(cld->testnet4) ?  28333 : 8333;
        // add it to the DB, making sure there is at least one.
        app->p2pNet()->connectionManager().peerAddressDb().addOne(
                    EndPoint(cld->parser.value(cld->connect).toStdString(), port));
    }

    app->startNet(); // lets go!
}
