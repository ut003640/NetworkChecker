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
    bool resolveDomainByIf(const QString& ifName, const QString& domain, in_addr& outIp, int timeout = 1000);
    bool checkNetCardOnline(const QString& ifName, const QString& domain, int timeout,
                             const QList<QPair<uint32_t, uint32_t>> &routes);
    bool isRouteViaIf(uint32_t targetIp, const QList<QPair<uint32_t, uint32_t>> &routes);
    QList<QPair<uint32_t, uint32_t>> loadRoutesForIf(const QString &ifName);
    bool hasDefaultRoute(const QList<QPair<uint32_t, uint32_t>> &routes);
};

#endif
