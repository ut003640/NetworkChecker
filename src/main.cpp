#include <QCoreApplication>
#include <csignal>
#include <unistd.h>

#include "networkchecker.h"
#include "networkcheckeradaptor.h"
#include "networkcheckerproxy.h"

static int sigPipeFds[2] = { -1, -1 };

static void signalHandler(int)
{
    char byte = 1;
    if (sigPipeFds[1] != -1) {
        ssize_t unused = ::write(sigPipeFds[1], &byte, sizeof(byte));
        (void)unused;
    }
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName("networkchecker");
    app.setOrganizationName("deepin");

    if (::pipe(sigPipeFds) != 0) {
        qCritical() << "Failed to create signal pipe";
        return 1;
    }
    QSocketNotifier signalNotifier(sigPipeFds[0], QSocketNotifier::Read);
    QObject::connect(&signalNotifier, &QSocketNotifier::activated, &app, &QCoreApplication::quit);

    std::signal(SIGTERM, signalHandler);
    std::signal(SIGINT, signalHandler);

    QDBusConnection connection = QDBusConnection::systemBus();
    if (!connection.registerService("org.deepin.demo.NetworkChecker")) {
        qCritical() << "Failed to register DBus service:" << connection.lastError().message();
        return 1;
    }

    NetworkCheckerProxy proxy;
    if (!connection.registerObject("/", &proxy)) {
        qCritical() << "Failed to register DBus object:" << connection.lastError().message();
        return 1;
    }

    NetworkCheckerAdaptor adapter(&proxy);
    qDebug() << "NetworkChecker service started on system bus";
    return app.exec();
}
