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
#include <chrono>

#include <QNetworkInterface>
#include <NetworkManagerQt/Manager>
#include <NetworkManagerQt/Device>
#include <NetworkManagerQt/IpConfig>

#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>

static const QStringList kTestHosts = {
    "https://223.5.5.5",
    "http://detect.uniontech.com",
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

    QStringList dnslist = getSysDnsList(interfacename);
    if (dnslist.isEmpty())
        return false;

    QStringList remote_addr;
    static const int timeout = 3000;
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
        in_addr addr;
        if (inet_pton(AF_INET, host.toStdString().c_str(), &addr) == 1) {
            const QString &remote_ip = QString::fromLatin1(inet_ntoa(addr));
            if (remote_addr.contains(remote_ip))
                continue;

            remote_addr << remote_ip;
            // 如果是IP地址，直接走IP地址检测
            sockaddr_in target;
            memset(&target, 0, sizeof(target));
            target.sin_family = AF_INET;
            target.sin_port = htons(80);
            target.sin_addr = addr;
            if (isIfaceReachable(interfacename, target, timeout)) {
                return true;
            }
        } else if (checkNetCardOnline(interfacename, host, dnslist, timeout, routes)) {
            // 如果是域名，就走域名检测
            return true;
        }
    }

    // 如果以上都无法检测网络是通的，就用兜底方案，直接检测公用的DNS
    static QStringList avalible_dns {"223.5.5.5", "223.6.6.6", "114.114.114.114", "8.8.8.8"};
    for (const QString &dns : avalible_dns) {
        if (remote_addr.contains(dns))
            continue;

        sockaddr_in target;
        memset(&target, 0, sizeof(target));
        target.sin_family = AF_INET;
        target.sin_port = htons(80);
        inet_pton(AF_INET, dns.toStdString().c_str(), &target.sin_addr);
        if (isIfaceReachable(interfacename, target, 1000)) {
            return true;
        }
    }
    return false;
}

bool NetworkCheckerByDomain::checkNetCardOnline(const QString &ifName, const QString &domain, const QStringList dnslist, int timeoutMs, const QList<QPair<uint32_t, uint32_t>> &routes)
{
    in_addr targetIp;
    if (!resolveByBindIface(ifName, domain, dnslist, targetIp, timeoutMs))
        return false;

    sockaddr_in dest;
    memset(&dest,0,sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons(80);
    dest.sin_addr = targetIp;

    return isIfaceReachable(ifName, dest, timeoutMs);
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

QStringList NetworkCheckerByDomain::getSysDnsList() const
{
    QStringList dnsList;
    QFile file("/etc/resolv.conf");
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QRegularExpression reg("^nameserver\\s+(\\d+\\.\\d+\\.\\d+\\.\\d+)");
        while (!file.atEnd()) {
            QString line = file.readLine().trimmed();
            QRegularExpressionMatch match = reg.match(line);
            if (match.hasMatch()) {
                dnsList << match.captured(1);
            }
        }
        file.close();
    }
    return dnsList;
}

QStringList NetworkCheckerByDomain::getSysDnsList(const QString &interface) const
{
    NetworkManager::Device::Ptr device;
    NetworkManager::Device::List deviceList = NetworkManager::networkInterfaces();
    for (const NetworkManager::Device::Ptr &d: deviceList) {
        QString ifname = d->interfaceName();
        if (ifname == interface) {
            device = d;
            break;
        }
    }

    if (device.isNull())
        return QStringList();

    NetworkManager::ActiveConnection::Ptr activeConnection = device->activeConnection();
    if (activeConnection.isNull())
        return QStringList();

    QStringList allDns;
    NetworkManager::IpConfig config = activeConnection->ipV4Config();
    QList<QHostAddress> nameServers = config.nameservers();
    for (const QHostAddress &host : nameServers) {
        QString ip = host.toString();
        if (!allDns.contains(ip))
            allDns << ip;
    }

    return allDns;
}

bool NetworkCheckerByDomain::isIfaceReachable(const QString &ifName, const sockaddr_in &dest, const int &timeoutMs) const
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

    int ret = ::connect(sockFd, (sockaddr*)&dest, sizeof(dest));
    if(ret == 0) {
        close(sockFd);
        return true;
    }
    if (errno == EINPROGRESS) {
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
            close(sockFd);
            return (err == 0);
        }
    }
    close(sockFd);
    return false;
}

bool NetworkCheckerByDomain::resolveByBindIface(const QString &ifName, const QString &domain, const QStringList dnslist, in_addr &outIp, int timeout)
{
    bool succ = false;
    int perDnsTimeout = qMax(timeout / qMax(dnslist.size(), 1), 1000);
    int remainTimeout = timeout;

    // 优先使用UDP的方式来检测
    for (const QString &dnsIp : dnslist) {
        if (succ || remainTimeout <= 0)
            break;

        if (checkIpAddrByUDP(ifName, dnsIp, domain, qMin(perDnsTimeout, remainTimeout), outIp)) {
            succ = true;
            break;
        }
    }

    if (!succ) {
        // 如果以上使用UDP的方式没有检测成功，再次使用TCP的方式，因为如果连接的是手机热点，可能是因为53端口被系统策略给屏蔽了，此时再使用TCP的方式来检测
        for (const QString &dnsIp : dnslist) {
            if (succ || remainTimeout <= 0)
                break;

            if (checkIpAddrByTCP(ifName, dnsIp, domain, qMin(perDnsTimeout, remainTimeout), outIp)) {
                succ = true;
                break;
            }
        }
    }

    if (!succ) {
        struct addrinfo hints, *res = nullptr;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        int gaiRet = getaddrinfo(domain.toStdString().c_str(), nullptr, &hints, &res);
        if (gaiRet == 0 && res != nullptr) {
            for (struct addrinfo *p = res; p != nullptr; p = p->ai_next) {
                if (p->ai_family == AF_INET) {
                    outIp = ((struct sockaddr_in *)p->ai_addr)->sin_addr;
                    freeaddrinfo(res);
                    return true;
                }
            }
            freeaddrinfo(res);
        }
    }
    return succ;
}

bool NetworkCheckerByDomain::checkIpAddrByUDP(const QString& ifName, const QString &dnsIp, const QString& domain, int curTimeout, in_addr &outIp) const
{
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
        return false;

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifName.toStdString().c_str(), IFNAMSIZ - 1);
    if (setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, &ifr, sizeof(ifr)) < 0) {
        close(sock);
        return false;
    }

    QList<QNetworkInterface> ifaces = QNetworkInterface::allInterfaces();
    bool bound = false;
    for (const QNetworkInterface &iface : ifaces) {
        if (iface.name() != ifName)
            continue;
        for (const QNetworkAddressEntry &entry : iface.addressEntries()) {
            if (entry.ip().protocol() != QAbstractSocket::IPv4Protocol)
                continue;
            sockaddr_in localAddr;
            memset(&localAddr, 0, sizeof(localAddr));
            localAddr.sin_family = AF_INET;
            localAddr.sin_port = 0;
            inet_pton(AF_INET, entry.ip().toString().toUtf8().constData(), &localAddr.sin_addr);
            if (::bind(sock, (sockaddr *)&localAddr, sizeof(localAddr)) == 0)
                bound = true;
            break;
        }
        break;
    }
    if (!bound) {
        close(sock);
        return false;
    }

    sockaddr_in dnsAddr;
    memset(&dnsAddr, 0, sizeof(dnsAddr));
    dnsAddr.sin_family = AF_INET;
    dnsAddr.sin_port = htons(53);
    if (inet_pton(AF_INET, dnsIp.toStdString().c_str(), &dnsAddr.sin_addr) <= 0) {
        close(sock);
        return false;
    }

    // 构造DNS A记录查询包
    unsigned char pkt[512] = {0};
    unsigned short id = htons(0x7788);
    memcpy(pkt, &id, 2);
    pkt[2] = 0x01; pkt[3] = 0x00;
    unsigned short qd = htons(1);
    memcpy(pkt + 4, &qd, 2);
    int ptr = 12;
    QStringList seg = domain.split(".");
    for (auto& s : seg) {
        QByteArray buf = s.toUtf8();
        pkt[ptr++] = static_cast<unsigned char>(buf.size());
        memcpy(pkt + ptr, buf.data(), buf.size());
        ptr += buf.size();
    }
    pkt[ptr++] = 0;
    unsigned short typeA = htons(1), cls = htons(1);
    memcpy(pkt + ptr, &typeA, 2); ptr += 2;
    memcpy(pkt + ptr, &cls, 2); ptr += 2;

    ssize_t sent = sendto(sock, pkt, ptr, MSG_NOSIGNAL, (sockaddr*)&dnsAddr, sizeof(dnsAddr));
    if (sent < 0) {
        qWarning() << "sendto failed for" << ifName << "dns:" << dnsIp << "errno:" << errno << strerror(errno);
        close(sock);
        return false;
    }

    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(sock, &rfds);
    timeval tv;
    tv.tv_sec = curTimeout / 1000;
    tv.tv_usec = (curTimeout % 1000) * 1000;

    bool succ = false;
    if (select(sock+1, &rfds, nullptr, nullptr, &tv) > 0) {
        char buf[512];
        ssize_t len = recv(sock, buf, sizeof(buf), 0);
        if (len > 12) {
            unsigned short respId;
            memcpy(&respId, buf, 2);
            respId = ntohs(respId);
            if (respId == 0x7788 && (buf[2] & 0x80)) {
                int p = 12;
                unsigned short qnum;
                memcpy(&qnum, buf + 4, 2);
                qnum = ntohs(qnum);

                unsigned short anNum;
                memcpy(&anNum, buf + 6, 2);
                anNum = ntohs(anNum);

                for (int i = 0; i < qnum; i++) {
                    while (p < len && buf[p] != 0) {
                        if ((buf[p] & 0xC0) == 0xC0) {
                            if (p + 2 > len)
                                goto skip_qtype_qclass;
                            p += 2;
                            goto skip_qtype_qclass;
                        }
                        p += buf[p] + 1;
                    }
                    if (p < len && buf[p] == 0)
                        p++;
                skip_qtype_qclass:
                    if (p + 4 > len)
                        break;
                    p += 4;
                }

                for (int i = 0; i < anNum && p + 12 <= len; i++) {
                    while (p < len && buf[p] != 0) {
                        if ((buf[p] & 0xC0) == 0xC0) {
                            if (p + 2 > len)
                                goto skip_ttl_rdlen;
                            p += 2;
                            goto skip_ttl_rdlen;
                        }
                        p += buf[p] + 1;
                    }
                    if (p < len && buf[p] == 0)
                        p++;
                skip_ttl_rdlen:
                    if (p + 10 > len)
                        break;

                    unsigned short rtype;
                    memcpy(&rtype, buf + p, 2);
                    rtype = ntohs(rtype);
                    p += 8;

                    unsigned short rdLen;
                    memcpy(&rdLen, buf + p, 2);
                    rdLen = ntohs(rdLen);
                    p += 2;

                    if (rtype == 1 && rdLen == 4 && p + 4 <= len) {
                        memcpy(&outIp, buf + p, 4);
                        succ = true;
                        break;
                    }
                    if (p + rdLen > len)
                        break;
                    p += rdLen;
                }
            }
        }
    }

    close(sock);
    return succ;
}

bool NetworkCheckerByDomain::checkIpAddrByTCP(const QString &ifName, const QString &dnsIp, const QString &domain, int curTimeout, in_addr &outIp) const
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
        return false;

     struct ifreq ifr;
     memset(&ifr, 0, sizeof(ifr));
     strncpy(ifr.ifr_name, ifName.toStdString().c_str(), IFNAMSIZ - 1);
     if (setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, &ifr, sizeof(ifr)) < 0) {
         close(sock);
         return false;
     }

     QList<QNetworkInterface> ifaces = QNetworkInterface::allInterfaces();
     bool bound = false;
     for (const QNetworkInterface &iface : ifaces) {
         if (iface.name() != ifName)
             continue;
         for (const QNetworkAddressEntry &entry : iface.addressEntries()) {
             if (entry.ip().protocol() != QAbstractSocket::IPv4Protocol)
                 continue;
             sockaddr_in localAddr;
             memset(&localAddr, 0, sizeof(localAddr));
             localAddr.sin_family = AF_INET;
             localAddr.sin_port = 0;
             inet_pton(AF_INET, entry.ip().toString().toUtf8().constData(), &localAddr.sin_addr);
             if (::bind(sock, (sockaddr *)&localAddr, sizeof(localAddr)) == 0)
                 bound = true;
             break;
         }
         break;
     }

     if (!bound) {
         close(sock);
         return false;
     }

     sockaddr_in dnsAddr;
     memset(&dnsAddr, 0, sizeof(dnsAddr));
     dnsAddr.sin_family = AF_INET;
     dnsAddr.sin_port = htons(53);
     if (inet_pton(AF_INET, dnsIp.toStdString().c_str(), &dnsAddr.sin_addr) <= 0) {
         close(sock);
         return false;
     }

     int flags = fcntl(sock, F_GETFL, 0);
     fcntl(sock, F_SETFL, flags | O_NONBLOCK);

     int connectRet = ::connect(sock, (sockaddr*)&dnsAddr, sizeof(dnsAddr));
     if (connectRet < 0 && errno != EINPROGRESS) {
         close(sock);
         return false;
     }

     fd_set wfds;
     FD_ZERO(&wfds);
     FD_SET(sock, &wfds);
     int connectTimeout = qMin(curTimeout, 2000);
     timeval connectTv;
     connectTv.tv_sec = connectTimeout / 1000;
     connectTv.tv_usec = (connectTimeout % 1000) * 1000;

     bool succ = false;
     int selRet = select(sock + 1, nullptr, &wfds, nullptr, &connectTv);
     if (selRet > 0) {
         int err = 0;
         socklen_t errLen = sizeof(err);
         getsockopt(sock, SOL_SOCKET, SO_ERROR, &err, &errLen);
         if (err == 0) {
             unsigned char pkt[512] = {0};
             unsigned short id = htons(0x7788);
             memcpy(pkt, &id, 2);
             pkt[2] = 0x01; pkt[3] = 0x00;
             unsigned short qd = htons(1);
             memcpy(pkt + 4, &qd, 2);
             int ptr = 12;
             QStringList seg = domain.split(".");
             for (auto& s : seg) {
                 QByteArray buf = s.toUtf8();
                 pkt[ptr++] = static_cast<unsigned char>(buf.size());
                 memcpy(pkt + ptr, buf.data(), buf.size());
                 ptr += buf.size();
             }
             pkt[ptr++] = 0;
             unsigned short typeA = htons(1), cls = htons(1);
             memcpy(pkt + ptr, &typeA, 2); ptr += 2;
             memcpy(pkt + ptr, &cls, 2); ptr += 2;

             unsigned char tcpPkt[514];
             unsigned short pktLen = htons(ptr);
             memcpy(tcpPkt, &pktLen, 2);
             memcpy(tcpPkt + 2, pkt, ptr);

             ssize_t sent = send(sock, tcpPkt, ptr + 2, MSG_NOSIGNAL);
             if (sent == ptr + 2) {
                 fd_set rfds;
                 FD_ZERO(&rfds);
                 FD_SET(sock, &rfds);
                 timeval recvTv;
                 recvTv.tv_sec = curTimeout / 1000;
                 recvTv.tv_usec = (curTimeout % 1000) * 1000;

                 if (select(sock + 1, &rfds, nullptr, nullptr, &recvTv) > 0) {
                     char respBuf[514];
                     ssize_t totalRecv = 0;
                     while (totalRecv < 514) {
                         ssize_t n = recv(sock, respBuf + totalRecv, 514 - totalRecv, 0);
                         if (n <= 0)
                             break;
                         totalRecv += n;
                     }
                     int parseOff = 0;
                     int parseLen = totalRecv;
                     if (totalRecv > 2) {
                         unsigned short hdrLen;
                         memcpy(&hdrLen, respBuf, 2);
                         hdrLen = ntohs(hdrLen);
                         if (hdrLen > 12 && hdrLen < 4096) {
                             parseOff = 2;
                             parseLen = hdrLen < (totalRecv - 2) ? hdrLen : (totalRecv - 2);
                         }
                     }
                     if (parseLen > 12) {
                         int p = parseOff + 12;
                         unsigned short qnum;
                         memcpy(&qnum, respBuf + parseOff + 4, 2);
                         qnum = ntohs(qnum);

                         unsigned short anNum;
                         memcpy(&anNum, respBuf + parseOff + 6, 2);
                         anNum = ntohs(anNum);

                         for (int i = 0; i < qnum; i++) {
                             while (p < parseOff + parseLen && respBuf[p] != 0) {
                                 if ((respBuf[p] & 0xC0) == 0xC0) {
                                     if (p + 2 > parseOff + parseLen)
                                         goto skip_qtype_qclass;
                                     p += 2;
                                     goto skip_qtype_qclass;
                                 }
                                 p += respBuf[p] + 1;
                             }
                             if (p < parseOff + parseLen && respBuf[p] == 0)
                                 p++;
                         skip_qtype_qclass:
                             if (p + 4 > parseOff + parseLen)
                                 break;
                             p += 4;
                         }

                         for (int i = 0; i < anNum && p + 12 <= parseOff + parseLen; i++) {
                             while (p < parseOff + parseLen && respBuf[p] != 0) {
                                 if ((respBuf[p] & 0xC0) == 0xC0) {
                                     if (p + 2 > parseOff + parseLen)
                                         goto skip_ttl_rdlen;
                                     p += 2;
                                     goto skip_ttl_rdlen;
                                 }
                                 p += respBuf[p] + 1;
                             }
                             if (p < parseOff + parseLen && respBuf[p] == 0)
                                 p++;
                         skip_ttl_rdlen:
                             if (p + 10 > parseOff + parseLen)
                                 break;

                             unsigned short rtype;
                             memcpy(&rtype, respBuf + p, 2);
                             rtype = ntohs(rtype);
                             p += 8;

                             unsigned short rdLen;
                             memcpy(&rdLen, respBuf + p, 2);
                             rdLen = ntohs(rdLen);
                             p += 2;

                             if (rtype == 1 && rdLen == 4 && p + 4 <= parseOff + parseLen) {
                                 memcpy(&outIp, respBuf + p, 4);
                                 succ = true;
                             }
                             if (p + rdLen > parseOff + parseLen)
                                 break;
                             p += rdLen;
                             if (succ)
                                 break;
                         }
                     }
                 }
             }
         }
    }

    close(sock);
    return succ;
}
