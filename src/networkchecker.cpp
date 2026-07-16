#include "networkchecker.h"

#include <sys/socket.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <netinet/in.h>
#include <cstring>
#include <netdb.h>

#include <QNetworkInterface>
#include <QDebug>
#include <QProcess>

NetworkChecker::NetworkChecker(QObject *parent)
    : QObject(parent)
{
}

bool NetworkChecker::isOnline(const QString &interfacename)
{
    if (interfacename.isEmpty()) {
        qWarning() << "Interface name is empty";
        return false;
    }

    if (!checkInterfaceExists(interfacename)) {
        qWarning() << "Interface does not exist:" << interfacename;
        return false;
    }

    return checkIfInterfaceOnline(interfacename);
}

bool NetworkChecker::checkInterfaceExists(const QString &interfacename)
{
    QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &iface : interfaces) {
        if (iface.name() == interfacename) {
            return true;
        }
    }
    return false;
}
