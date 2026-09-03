#ifndef NETWORKCHECKERBYDOMAIN_H
#define NETWORKCHECKERBYDOMAIN_H

#include "networkchecker.h"

#include <NetworkManagerQt/Device>

class NetworkCheckerByDomain : public NetworkChecker
{
    Q_OBJECT

public:
    explicit NetworkCheckerByDomain(QObject *parent = nullptr);

protected:
    bool checkIfInterfaceOnline(const QString &interfacename) override;

private:
    QStringList getDeviceDnsList(const NetworkManager::Device::Ptr &device) const;
    bool isIfaceReachable(const QString &ifName, const sockaddr_in &dest, int timeoutMs) const;
    bool checkNetCardOnline(const NetworkManager::Device::Ptr &device, const QString &domain, const QStringList &dnslist, int timeoutMs, int port) const;
    bool resolveByBindIface(const NetworkManager::Device::Ptr &device, const QString &domain, const QStringList &dnslist, in_addr &outIp, int timeout) const;
    bool checkIpAddrByUDP(const NetworkManager::Device::Ptr &device, const QString &dnsIp, const QString &domain, int curTimeout, in_addr &outIp) const;
    bool checkIpAddrByTCP(const NetworkManager::Device::Ptr &device, const QString &dnsIp, const QString &domain, int curTimeout, in_addr &outIp) const;
    int buildDnsQueryPacket(const QString &domain, unsigned char *pkt, int pktSize, unsigned short txId) const;
    bool parseDnsResponse(const unsigned char *buf, int len, unsigned short txId, in_addr &outIp) const;
};

#endif
