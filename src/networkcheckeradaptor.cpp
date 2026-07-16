#include "networkcheckeradaptor.h"
#include "networkcheckerbyip.h"
#include "networkcheckerbydomain.h"
#include "networkcheckerproxy.h"

NetworkCheckerAdaptor::NetworkCheckerAdaptor(NetworkCheckerProxy *parent)
    : QDBusAbstractAdaptor(parent)
    , m_checkerProxy(parent)
{
}

NetworkCheckerAdaptor::~NetworkCheckerAdaptor()
{
}

bool NetworkCheckerAdaptor::isOnline(const QString &interfacename, bool byHostname)
{
    return m_checkerProxy->isOnline(interfacename, byHostname);
}
