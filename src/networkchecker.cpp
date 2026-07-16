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
        if (pingThroughInterface(interfacename, host)) {
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

bool NetworkChecker::pingThroughInterface(const QString &interfacename, const QString &host)
{
    QProcess proc;
    proc.setProcessChannelMode(QProcess::MergedChannels);
    proc.start("ping", {"-I", interfacename, "-c", "1", "-W", "3", host});
    proc.waitForFinished(5000);

    return proc.exitCode() == 0;
}

bool NetworkChecker::httpCheckThroughInterface(const QString &interfacename, const QString &url)
{
    // 1. 创建UDP socket（轻量，比TCP快）
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if(sock < 0) return false;

    // 2. 设置非阻塞 + 超时
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    // 3. 绑定指定网卡核心：SO_BINDTODEVICE
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, interfacename.toStdString().c_str(), IFNAMSIZ-1);
    if(setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, &ifr, sizeof(ifr)) < 0)
    {
        close(sock);
        qWarning() << "绑定网卡失败" << interfacename << strerror(errno);
        return false;
    }

    // 4. 目标外网DNS：8.8.8.8 端口53
    sockaddr_in target;
    memset(&target, 0, sizeof(target));
    target.sin_family = AF_INET;
    target.sin_port = htons(53);
    inet_pton(AF_INET, "8.8.8.8", &target.sin_addr);

    // 5. 发送空包测试连通路由
    sendto(sock, "test", 4, MSG_NOSIGNAL, (sockaddr*)&target, sizeof(target));

    // 6. select 超时等待回复
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(sock, &fds);
    timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 500 * 1000;

    bool online = false;
    if(select(sock+1, &fds, nullptr, nullptr, &tv) > 0)
    {
        char buf[64];
        if(recv(sock, buf, sizeof(buf), MSG_DONTWAIT) > 0)
            online = true;
    }

    close(sock);
    return online;
}
