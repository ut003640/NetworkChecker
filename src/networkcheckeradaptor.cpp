#include "networkcheckeradaptor.h"
#include "networkchecker.h"

NetworkCheckerAdaptor::NetworkCheckerAdaptor(NetworkChecker *parent)
    : QDBusAbstractAdaptor(parent)
{
    setAutoRelaySignals(true);
}

NetworkCheckerAdaptor::~NetworkCheckerAdaptor()
{
}

NetworkChecker *NetworkCheckerAdaptor::parent() const
{
    return static_cast<NetworkChecker *>(QObject::parent());
}

bool NetworkCheckerAdaptor::isOnline(const QString &interfacename)
{
    return parent()->isOnline(interfacename);
}
