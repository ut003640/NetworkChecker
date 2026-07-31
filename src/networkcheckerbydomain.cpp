#include "networkcheckerbydomain.h"

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
#include <QFile>
#include <QTextStream>

static const QStringList kTestHosts = {
    "http://detect.uniontech.com",
    "http://www.baidu.com"
};

NetworkCheckerByDomain::NetworkCheckerByDomain(QObject *parent)
    : NetworkChecker(parent)
{
}

bool NetworkCheckerByDomain::checkIfInterfaceOnline(const QString &interfacename)
{
    auto routes = loadRoutesForIf(interfacename);

    if (!hasDefaultRoute(routes))
        return false;

    for (const QString &hostname : kTestHosts) {
        QString host = hostname;
        static QStringList headers = {"https://", "http://"};
        for (const QString &current_header : headers) {
            if (host.startsWith(current_header)) {
                host = host.remove(0, current_header.length());
                break;
            }
        }
        if (host.endsWith("/")) {
            host = host.left(host.length() - 1);
        }
        if (checkNetCardOnline(interfacename, host, 5000, routes))
            return true;
    }

    return false;
}

bool NetworkCheckerByDomain::checkNetCardOnline(const QString &ifName, const QString &domain, int timeoutMs, const QList<QPair<uint32_t, uint32_t>> &routes)
{
    // 1. 创建TCP socket
    int sockFd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (sockFd < 0) {
        return false;
    }

    // 2. 强制绑定网卡设备，所有流量从该网卡发出
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifName.toStdString().c_str(), IFNAMSIZ - 1);
    if (setsockopt(sockFd, SOL_SOCKET, SO_BINDTODEVICE, &ifr, sizeof(ifr)) < 0) {
        close(sockFd);
        return false;
    }

    // 2.1 绑定网卡源IP，防止多网卡环境下非对称路由导致回包走错网卡
    QList<QNetworkInterface> ifaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &iface : ifaces) {
        if (iface.name() == ifName) {
            for (const QNetworkAddressEntry &entry : iface.addressEntries()) {
                if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                    sockaddr_in localAddr;
                    memset(&localAddr, 0, sizeof(localAddr));
                    localAddr.sin_family = AF_INET;
                    localAddr.sin_port = 0;
                    inet_pton(AF_INET, entry.ip().toString().toStdString().c_str(), &localAddr.sin_addr);
                    if (::bind(sockFd, (sockaddr *)&localAddr, sizeof(localAddr)) < 0) {
                        close(sockFd);
                        return false;
                    }
                    break;
                }
            }
            break;
        }
    }

    // 3. 解析域名地址（无固定DNS IP，系统自动使用网卡对应DNS）
    struct addrinfo hints, *res = nullptr;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int gaiRet = getaddrinfo(domain.toStdString().c_str(), "80", &hints, &res);
    if (gaiRet != 0 || res == nullptr) {
        close(sockFd);
        return false;
    }

    // 4. 遍历域名所有IPv4地址尝试连接
    bool connectOk = false;
    for (struct addrinfo* p = res; p != nullptr; p = p->ai_next) {
#ifdef QT_DEBUG
        char ipStr[INET_ADDRSTRLEN];
        auto *addr = (struct sockaddr_in *)p->ai_addr;
        inet_ntop(AF_INET, &addr->sin_addr, ipStr, sizeof(ipStr));
        qInfo() << "ip address:" << ipStr;
#endif
        char targetIp[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &((sockaddr_in *)p->ai_addr)->sin_addr, targetIp, sizeof(targetIp));

        in_addr rawIp;
        inet_pton(AF_INET, targetIp, &rawIp);
        if (!isRouteViaIf(rawIp.s_addr, routes)) {
#ifdef QT_DEBUG
            qInfo() << "skip ip, route not via" << ifName << ":" << targetIp;
#endif
            continue;
        }

        if (::connect(sockFd, p->ai_addr, p->ai_addrlen) == 0) {
            connectOk = true;
            break;
        }
        // 非阻塞connect必然返回EINPROGRESS，正常现象
        if (errno != EINPROGRESS)
            continue;

        // 5. select 等待连接超时
        fd_set wfds;
        FD_ZERO(&wfds);
        FD_SET(sockFd, &wfds);
        timeval tv;
        tv.tv_sec = timeoutMs / 1000;
        tv.tv_usec = (timeoutMs % 1000) * 1000;

        int selRet = select(sockFd + 1, nullptr, &wfds, nullptr, &tv);
        if (selRet > 0) {
            // 检查是否连接成功
            int err = 0;
            socklen_t errLen = sizeof(err);
            getsockopt(sockFd, SOL_SOCKET, SO_ERROR, &err, &errLen);
            if (err == 0) {
                connectOk = true;
                break;
            }
        }
    }

    freeaddrinfo(res);
    close(sockFd);
    return connectOk;
}

bool NetworkCheckerByDomain::isRouteViaIf(uint32_t targetIp, const QList<QPair<uint32_t, uint32_t>> &routes)
{
#ifdef QT_DEBUG
    char destStr[INET_ADDRSTRLEN], maskStr[INET_ADDRSTRLEN], targetStr[INET_ADDRSTRLEN];
    in_addr addr;
#endif
    for (const auto &route : routes) {
#ifdef QT_DEBUG
        addr.s_addr = route.first;
        inet_ntop(AF_INET, &addr, destStr, sizeof(destStr));
        addr.s_addr = route.second;
        inet_ntop(AF_INET, &addr, maskStr, sizeof(maskStr));
        addr.s_addr = targetIp;
        inet_ntop(AF_INET, &addr, targetStr, sizeof(targetStr));
        qDebug() << "route dest:" << destStr << "mask:" << maskStr << "target:" << targetStr;
#endif
        if ((targetIp & route.second) == route.first)
            return true;
    }
    return false;
}

QList<QPair<uint32_t, uint32_t>> NetworkCheckerByDomain::loadRoutesForIf(const QString &ifName)
{
    QList<QPair<uint32_t, uint32_t>> routes;
    QFile f("/proc/net/route");
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return routes;

    QTextStream in(&f);
    in.readLine();
    while (!in.atEnd()) {
        QString line = in.readLine();
        QStringList fields = line.split(QChar('\t'));
        if (fields.size() < 8 || fields[0] != ifName)
            continue;

        bool ok = false;
        uint32_t dest = fields[1].toUInt(&ok, 16);
        if (!ok) continue;
        uint32_t mask = fields[7].toUInt(&ok, 16);
        if (!ok) continue;

        routes.append({htonl(dest), htonl(mask)});
    }
    return routes;
}

bool NetworkCheckerByDomain::hasDefaultRoute(const QList<QPair<uint32_t, uint32_t>> &routes)
{
    for (const auto &route : routes) {
        if (route.first == 0 && route.second == 0)
            return true;
    }
    return false;
}
