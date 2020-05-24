#include "NetDataProvider.h"
#include "PortfolioDataProvider.h"
#include "FloweePay.h"

#include <QCommandLineParser>
#include <QDateTime>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QStandardPaths>

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
    qapp.setApplicationVersion("0.2");

    srand((quint32) QDateTime::currentMSecsSinceEpoch());

    qmlRegisterType<FloweePay>("Flowee.org.pay", 1, 0, "Pay");
    QCommandLineParser parser;
    parser.setApplicationDescription("Flowee pay");
    QCommandLineOption debug(QStringList() << "debug", "Use debug level logging");
    QCommandLineOption verbose(QStringList() << "verbose" << "v", "Be more verbose");
    QCommandLineOption quiet(QStringList() << "quiet" << "q", "Be quiet, only errors are shown");
    QCommandLineOption connect(QStringList() << "connect", "Connect to HOST", "HOST");
    parser.addHelpOption();
    parser.addOption(debug);
    parser.addOption(verbose);
    parser.addOption(quiet);
    parser.addOption(connect);
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

    ECC_State crypo_state; // allows the secp256k1 to function.
    QQmlApplicationEngine engine;
    const QUrl url(QStringLiteral("qrc:/main.qml"));
    engine.rootContext()->setContextProperty("loading", QVariant(true));
    engine.rootContext()->setContextProperty("Flowee", FloweePay::instance());
    engine.load(url);

    QObject::connect(FloweePay::instance(), &FloweePay::loadComplete, [&engine, &parser, &connect]() {
        FloweePay *app = FloweePay::instance();

        NetDataProvider *netData = new NetDataProvider(app->p2pNet()->blockHeight(), &engine);
        app->p2pNet()->addP2PNetListener(netData);

        PortfolioDataProvider *wallets = new PortfolioDataProvider(&engine);
        for (auto wallet : app->wallets()) {
            wallets->addWalletAccount(wallet);
        }
        engine.rootContext()->setContextProperty("net", netData);
        engine.rootContext()->setContextProperty("wallets", wallets);
        engine.rootContext()->setContextProperty("loading", QVariant(false));
        if (parser.isSet(connect)) {
            app->p2pNet()->connectionManager().peerAddressDb().addOne( // actually connect to it too.
                        EndPoint(parser.value(connect).toStdString(), 8333));
        }
        app->p2pNet()->start(); // lets go!
    });

    struct sigaction sa;
    sa.sa_handler = HandleSigTerm;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGINT, &sa, nullptr);

    return qapp.exec();
}
