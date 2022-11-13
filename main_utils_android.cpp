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
};

CommandLineParserData* createCLD(QGuiApplication &)
{
    return new CommandLineParserData();
}

Log::Verbosity logVerbosity(CommandLineParserData*)
{
    return  Log::FatalLevel;
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
    QFileInfo infTarget("staticHeaders.info");
    QFileInfo orig("assets:/blockheaders");
    QFileInfo infOrig("assets:/blockheaders.info");

    if (!orig.exists() || !infOrig.exists()) {
        logFatal() << "MIssing blockheader assets";
        abort();
    }

    bool install = !target.exists() || !infTarget.exists() || orig.size() != target.size();
    if (install) {
        // make sure we have a local up-to-date copy
        QFile::copy(orig.filePath(), target.absoluteFilePath());
        QFile::copy(orig.filePath() + ".info", infTarget.absoluteFilePath());
    }
    assert(target.exists());
    assert(infTarget.exists());

    blockheaders.reset(new QFile(target.absoluteFilePath()));
    if (!blockheaders->open(QIODevice::ReadOnly)) { // can't be opened for reading.
        blockheaders.reset();
        return blockheaders;
    }
    Blockchain::setStaticChain(blockheaders->map(0, blockheaders->size()), blockheaders->size(),
                              infTarget.absoluteFilePath().toStdString());
    blockheaders->close();
    return blockheaders;
}

void loadCompleteHandler(QQmlApplicationEngine &engine, CommandLineParserData*)
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
    app->startNet(); // lets go!
}
