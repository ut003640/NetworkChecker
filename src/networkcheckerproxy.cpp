#include "networkcheckerproxy.h"
#include "networkcheckerbydomain.h"
#include "networkcheckerbyip.h"

NetworkCheckerProxy::NetworkCheckerProxy(QObject *parent)
    : QObject(parent)
{
}

bool NetworkCheckerProxy::isOnline(const QString &interfacename, bool byHostname)
{
    if (byHostname) {
        m_checker.reset(new NetworkCheckerByDomain);
    } else {
        m_checker.reset(new NetworkCheckerByIp);
    }
    return m_checker->isOnline(interfacename);
}
