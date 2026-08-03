#ifndef NETWORKCHECKERBYDOMAIN_H
#define NETWORKCHECKERBYDOMAIN_H

#include "networkchecker.h"

#include <sys/socket.h>
#include <net/if.h>
#include <arpa/inet.h>

class NetworkCheckerByDomain : public NetworkChecker
{
    Q_OBJECT

public:
    explicit NetworkCheckerByDomain(QObject *parent = nullptr);

protected:
    bool checkIfInterfaceOnline(const QString &interfacename) override;

private:
    bool checkNetCardOnline(const QString& ifName, const QString& domain, const QStringList dnslist, int timeout, const QList<QPair<uint32_t, uint32_t>> &routes);
    bool isRouteViaIf(uint32_t targetIp, const QList<QPair<uint32_t, uint32_t>> &routes);
    QList<QPair<uint32_t, uint32_t>> loadRoutesForIf(const QString &ifName);
    bool hasDefaultRoute(const QList<QPair<uint32_t, uint32_t>> &routes);
    QStringList getSysDnsList() const;
    QStringList getSysDnsList(const QString &interface) const;
    bool isIfaceReachable(const QString &ifName, const sockaddr_in &addr, const int &timeoutMs) const;
    bool resolveByBindIface(const QString& ifName, const QString& domain, const QStringList dnslist, in_addr& outIp, int timeout);

    bool checkIpAddrByUDP(const QString& ifName, const QString &dnsIp, const QString& domain, int curTimeout, in_addr& outIp) const;
    bool checkIpAddrByTCP(const QString& ifName, const QString &dnsIp, const QString& domain, int curTimeout, in_addr& outIp) const;

};

#endif
