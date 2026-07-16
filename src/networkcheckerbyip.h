#ifndef NETWORKCHECKERBYIP_H
#define NETWORKCHECKERBYIP_H

#include "networkchecker.h"

class NetworkCheckerByIp : public NetworkChecker
{
    Q_OBJECT

public:
    explicit NetworkCheckerByIp(QObject *parent = nullptr);

protected:
    bool checkIfInterfaceOnline(const QString &interfacename) override;

private:
    bool tcpCheckThroughInterface(const QString &interfacename, const QString &host);
};

#endif
