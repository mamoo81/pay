/*
 * This file is part of the Flowee project
 * Copyright (C) 2020-2022 Tom Zander <tom@flowee.org>
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
#include "NewWalletConfig.h"
#include "PriceDataProvider.h"
#include "Payment.h"
#include "TransactionInfo.h"
#include "PaymentRequest.h"
#include "QRCreator.h"
#include "MenuModel.h"
#ifdef NETWORK_LOGGER
# include "NetworkLogClient.h"
#endif
#ifdef MOBILE
#include "CameraController.h"
#include "QRScanner.h"
#endif

#include <primitives/key.h> // for ECC_Start()

#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
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
        globalVerifyHandle.reset(new ECCVerifyHandle());
    }
    ~ECC_State() {
        globalVerifyHandle.reset();
        ECC_Stop();
    }

    std::unique_ptr<ECCVerifyHandle> globalVerifyHandle;
};
}

#ifdef TARGET_OS_Android
# include "main_utils_android.cpp"
#else
# include "main_utils.cpp"
#endif


// defined in qml_path_helper.cpp.in
void handleLocalQml(QQmlApplicationEngine &engine);
// defined in the utils file
struct CommandLineParserData;
CommandLineParserData* createCLD(QGuiApplication &app);
void loadCompleteHandler(QQmlApplicationEngine &engine, CommandLineParserData *cld);
std::unique_ptr<QFile> handleStaticChain(CommandLineParserData *cld);
Log::Verbosity logVerbosity(CommandLineParserData *cld);


int main(int argc, char *argv[])
{
    QGuiApplication qapp(argc, argv);
    qapp.setOrganizationName("flowee");
    qapp.setApplicationName("pay");
    qapp.setApplicationVersion("2023.05.0");
    qapp.setWindowIcon(QIcon(":/FloweePay.png"));

    qmlRegisterType<WalletEnums>("Flowee.org.pay", 1, 0, "Wallet");
    qmlRegisterType<BitcoinValue>("Flowee.org.pay", 1, 0, "BitcoinValue");
    qmlRegisterType<NewWalletConfig>("Flowee.org.pay", 1, 0, "NewWalletConfig");
    qmlRegisterType<Payment>("Flowee.org.pay", 1, 0, "Payment");
    qmlRegisterType<PaymentRequest>("Flowee.org.pay", 1, 0, "PaymentRequest");

    auto cld = createCLD(qapp);
    auto *logger = Log::Manager::instance();
    logger->clearChannels();
    logger->clearLogLevels(logVerbosity(cld));
    logger->addConsoleChannel();
#ifdef NETWORK_LOGGER
    logger->addChannel(new NetworkLogClient(FloweePay::instance()->ioService()));
#endif

    static const char* languagePacks[] = {
#ifdef DESKTOP
        "floweepay-desktop",
#elif MOBILE
        "floweepay-mobile",
#endif
        "floweepay-common",
        nullptr
    };

    for (int i = 0; languagePacks[i]; ++i) {
        auto *translator = new QTranslator(&qapp);
        if (translator->load(QLocale(), languagePacks[i], QLatin1String("_"), QLatin1String(":/i18n")))
            QCoreApplication::installTranslator(translator);
        else
            delete translator;
    }

    ECC_State crypo_state; // allows the secp256k1 to function.
    qmlRegisterType<TransactionInfo>("Flowee.org.pay", 1, 0, "TransactionInfo");
    qmlRegisterType<PaymentRequest>("Flowee.org.pay", 1, 0, "PaymentRequest");
    QQmlApplicationEngine engine;
#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
    // quit on error in the QMLs
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &qapp, [=]() {
        logFatal() << "QML has errors, aborting on purpose now";
        abort();
    }, Qt::QueuedConnection);
#endif

    engine.addImageProvider(QLatin1String("qr"), new QRCreator());
    auto app = FloweePay::instance();
    engine.rootContext()->setContextProperty("Pay", app);
    engine.rootContext()->setContextProperty("Fiat", app->prices());
    MenuModel menuModel;
    engine.rootContext()->setContextProperty("MenuModel", &menuModel);

#ifdef MOBILE
    qmlRegisterType<QRScanner>("Flowee.org.pay", 1, 0, "QRScanner");
    CameraController *cc = new CameraController(app);
    app->setCameraController(cc);
    engine.rootContext()->setContextProperty("CameraController", cc);
#endif

    handleLocalQml(engine);
    engine.load(engine.baseUrl().url() +
#ifdef DESKTOP
        "/desktop"
#elif MOBILE
        "/mobile"
#endif
                "/main.qml");

    auto blockheaders = handleStaticChain(cld);
    QObject::connect(FloweePay::instance(), &FloweePay::loadComplete, &engine, [&engine, &cld]() {
        loadCompleteHandler(engine, cld);
    });
    FloweePay::instance()->startP2PInit();

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
