#include "networkchecker.h"
#include "networkcheckeradaptor.h"

#include <sys/socket.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <QNetworkInterface>
#include <QDebug>
#include <QProcess>

static const QStringList kTestHosts = {
    "8.8.8.8",
    "114.114.114.114",
    "223.5.5.5"
};

NetworkChecker::NetworkChecker(QObject *parent)
    : QObject(parent)
{
    new NetworkCheckerAdaptor(this);
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

    for (const QString &host : kTestHosts) {
        if (tcpCheckThroughInterface(interfacename, host)) {
            qDebug() << "Interface" << interfacename << "is online via" << host;
            return true;
        }
    }

    qWarning() << "Interface" << interfacename << "is offline";
    return false;
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

bool NetworkChecker::tcpCheckThroughInterface(const QString &interfacename, const QString &host)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return false;

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, interfacename.toStdString().c_str(), IFNAMSIZ - 1);
    if (setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, &ifr, sizeof(ifr)) < 0) {
        close(sock);
        qWarning() << "bind device failed" << interfacename << strerror(errno);
        return false;
    }

    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    sockaddr_in target;
    memset(&target, 0, sizeof(target));
    target.sin_family = AF_INET;
    target.sin_port = htons(53);
    inet_pton(AF_INET, host.toStdString().c_str(), &target.sin_addr);

    int ret = ::connect(sock, (sockaddr *)&target, sizeof(target));
    bool online = false;

    if (ret == 0) {
        online = true;
    } else if (errno == EINPROGRESS) {
        fd_set wfds;
        FD_ZERO(&wfds);
        FD_SET(sock, &wfds);
        timeval tv;
        tv.tv_sec = 5;
        tv.tv_usec = 0;

        if (select(sock + 1, nullptr, &wfds, nullptr, &tv) > 0) {
            int err = 0;
            socklen_t len = sizeof(err);
            getsockopt(sock, SOL_SOCKET, SO_ERROR, &err, &len);
            online = (err == 0);
        }
    }

    close(sock);
    return online;
}
