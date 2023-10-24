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
#include "FloweePay.h"
#include "PortfolioDataProvider.h"
#include "NetDataProvider.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QFileInfo>
#include <qqmlcontext.h>

#include <P2PNet.h>

struct CommandLineParserData
{
    QStringList payRequest;
};

CommandLineParserData* createCLD(QGuiApplication &app)
{
    auto dat = new CommandLineParserData();
    // disable this for now, the argument has some weird stuff passed in
    // upon normal starting of the app.
    // Need to figure out how to do this the android style...
    // dat->payRequest = app.arguments();
    return dat;
}

Log::Verbosity logVerbosity(CommandLineParserData*)
{
#ifdef NETWORK_LOGGER
    return  Log::DebugLevel;
#else
    return  Log::FatalLevel;
#endif
}

std::unique_ptr<QFile> handleStaticChain(CommandLineParserData*)
{
    std::unique_ptr<QFile> blockheaders; // pointer to own the memmapped blockheaders file.

    /*
     * On Android the way to get bigger files into the app is to use the 'assets' subsystem.
     * Android is smart and doesn't actually install those files, just provides an interface
     * to the still compressed archive.
     *
     * Unfortunately, that means we can't memory map them and as such we need to do a one-time
     * copy of those files to our private homedir.
     */
    QFileInfo target("staticHeaders");
    QFileInfo orig("assets:/blockheaders");
    const QString infoFilePath = target.absoluteFilePath() + ".info";

    bool install = orig.exists() && (!target.exists() || orig.size() > target.size());
    if (install) {
        // make sure we have a local copy
        QFile::remove(infoFilePath);
        QFile::remove(target.absoluteFilePath());
        bool ok = QFile::copy(orig.filePath(), target.absoluteFilePath());
        if (!ok) {
            logFatal() << "Failed copying blockheaders";
            abort();
        }
    }
    if (!target.exists()) // We didn't find a source for the headers
        abort();

    blockheaders.reset(new QFile(target.absoluteFilePath()));
    if (!blockheaders->open(QIODevice::ReadOnly)) { // if can't be opened for reading.
        blockheaders.reset();
        return blockheaders;
    }

    // if not previously created, create metadata file now.
    if (!QFile::exists(infoFilePath))
        Blockchain::createStaticHeaders(blockheaders->fileName().toStdString(),
                                        infoFilePath.toStdString());
    Blockchain::setStaticChain(blockheaders->map(0, blockheaders->size()), blockheaders->size(),
                              infoFilePath.toStdString());
    blockheaders->close();
    return blockheaders;
}

void loadCompleteHandler(QQmlApplicationEngine &engine, CommandLineParserData *cld)
{
    FloweePay *app = FloweePay::instance();
    if (cld->payRequest.size() == 1)
        app->setPaymentProtocolRequest(cld->payRequest.first());
    NetDataProvider *netData = new NetDataProvider(&engine);
    app->p2pNet()->addP2PNetListener(netData);
    netData->startRefreshTimer();

    PortfolioDataProvider *portfolio = new PortfolioDataProvider(&engine);
    engine.rootContext()->setContextProperty("net", netData);
    engine.rootContext()->setContextProperty("portfolio", portfolio);
    app->startNet(); // lets go!
}
