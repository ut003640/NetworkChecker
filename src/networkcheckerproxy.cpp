#include "networkcheckerproxy.h"
#include "networkcheckerbydomain.h"
#include "networkcheckerbyip.h"

#include <QDebug>

#include <NetworkManagerQt/Manager>
#include <NetworkManagerQt/Connection>
#include <NetworkManagerQt/ConnectionSettings>
#include <NetworkManagerQt/Settings>
#include <NetworkManagerQt/Ipv4Setting>
#include <NetworkManagerQt/Ipv6Setting>

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

static quint32 defaultMetricForDeviceType(NetworkManager::Device::Type type)
{
    switch (type) {
    case NetworkManager::Device::Ethernet:   return 100;
    case NetworkManager::Device::Wifi:       return 600;
    case NetworkManager::Device::Bond:       return 300;
    case NetworkManager::Device::Team:       return 350;
    case NetworkManager::Device::Vlan:       return 400;
    case NetworkManager::Device::Bridge:     return 425;
    case NetworkManager::Device::VxLan:      return 500;
    case NetworkManager::Device::Modem:      return 700;
    case NetworkManager::Device::Bluetooth:  return 750;
    case NetworkManager::Device::Dummy:      return 550;
    default:                                 return 950;
    }
}

static QSet<QString> activeInterfaceNames()
{
    QSet<QString> result;
    NetworkManager::ActiveConnection::List activeConns = NetworkManager::activeConnections();
    for (const NetworkManager::ActiveConnection::Ptr &ac : activeConns) {
        if (ac->state() != NetworkManager::ActiveConnection::Activated)
            continue;
        NetworkManager::Device::Ptr dev = ac->devices().isEmpty()
            ? nullptr
            : NetworkManager::findNetworkInterface(ac->devices().first());
        if (dev)
            result.insert(dev->interfaceName());
    }
    return result;
}

static QHash<QString, quint32> buildDeviceMetrics()
{
    QHash<QString, quint32> result;
    NetworkManager::Device::List devices = NetworkManager::networkInterfaces();
    for (const NetworkManager::Device::Ptr &dev : devices) {
        result[dev->interfaceName()] = defaultMetricForDeviceType(dev->type());
    }
    return result;
}

bool NetworkCheckerProxy::setPrimaryInterface(const QString &interfacename)
{
    auto activeIfaces = activeInterfaceNames();
    if (!activeIfaces.contains(interfacename)) {
        qWarning() << "Interface not active:" << interfacename;
        return false;
    }

    auto deviceMetrics = buildDeviceMetrics();
    NetworkManager::Connection::List connections = NetworkManager::listConnections();

    for (const NetworkManager::Connection::Ptr &conn : connections) {
        NetworkManager::ConnectionSettings::Ptr settings = conn->settings();
        QString ifaceName = settings->interfaceName();

        if (ifaceName.isEmpty())
            continue;

        bool isTarget = (ifaceName == interfacename);
        if (!isTarget && !activeIfaces.contains(ifaceName))
            continue;

        quint32 metric = isTarget
            ? deviceMetrics.value(ifaceName, 100)
            : deviceMetrics.value(ifaceName, 950);

        NetworkManager::Ipv4Setting::Ptr ipv4 = settings->setting(NetworkManager::Setting::Ipv4)
            .staticCast<NetworkManager::Ipv4Setting>();
        if (ipv4) {
            ipv4->setRouteMetric(metric);
            ipv4->setInitialized(true);
        }

        NetworkManager::Ipv6Setting::Ptr ipv6 = settings->setting(NetworkManager::Setting::Ipv6)
            .staticCast<NetworkManager::Ipv6Setting>();
        if (ipv6) {
            ipv6->setRouteMetric(metric);
            ipv6->setInitialized(true);
        }

        conn->update(settings->toMap());
    }

    return true;
}
