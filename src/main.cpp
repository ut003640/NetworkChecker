#include <QCoreApplication>
#include <QDebug>
#include <QDBusConnection>
#include <QDBusError>
#include <csignal>

#include "networkchecker.h"

static void signalHandler(int)
{
    QCoreApplication::quit();
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName("networkchecker");
    app.setOrganizationName("deepin");

    std::signal(SIGTERM, signalHandler);
    std::signal(SIGINT, signalHandler);

    QDBusConnection connection = QDBusConnection::systemBus();
    if (!connection.registerService("org.deepin.demo.NetworkChecker")) {
        qCritical() << "Failed to register DBus service:"
                     << connection.lastError().message();
        return 1;
    }

    NetworkChecker checker;
    if (!connection.registerObject("/", &checker)) {
        qCritical() << "Failed to register DBus object:"
                     << connection.lastError().message();
        return 1;
    }

    qDebug() << "NetworkChecker service started on system bus";
    return app.exec();
}
