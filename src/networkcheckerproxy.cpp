#include "networkcheckerproxy.h"
#include "networkcheckerbydomain.h"
#include "networkcheckerbyip.h"

#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusArgument>
#include <QDebug>

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

static QMap<QString, quint64> getActiveConnections()
{
    QMap<QString, quint64> result;
    QDBusInterface nm("org.freedesktop.NetworkManager",
                       "/org/freedesktop/NetworkManager",
                       "org.freedesktop.NetworkManager",
                       QDBusConnection::systemBus());
    if (!nm.isValid())
        return result;

    QDBusReply<QList<QDBusObjectPath>> reply = nm.call("GetActiveConnections");
    if (!reply.isValid())
        return result;

    for (const QDBusObjectPath &path : reply.value()) {
        QDBusInterface conn("org.freedesktop.NetworkManager",
                            path.path(),
                            "org.freedesktop.NetworkManager.Connection.Active",
                            QDBusConnection::systemBus());
        if (!conn.isValid())
            continue;

        QList<QDBusObjectPath> devices = conn.property("Devices").value<QList<QDBusObjectPath>>();
        if (devices.isEmpty())
            continue;
        QDBusInterface device("org.freedesktop.NetworkManager",
                              devices.first().path(),
                              "org.freedesktop.DBus.Properties",
                              QDBusConnection::systemBus());
        QDBusReply<QVariant> ifaceReply = device.call("Get", "org.freedesktop.NetworkManager.Device", "Interface");
        QString ifaceName = ifaceReply.value().toString();

        quint64 state = conn.property("State").value<quint64>();
        result[ifaceName] = state;
    }
    return result;
}

static QMap<QString, quint32> getDeviceDefaultMetrics()
{
    QMap<QString, quint32> result;
    QDBusInterface nm("org.freedesktop.NetworkManager",
                       "/org/freedesktop/NetworkManager",
                       "org.freedesktop.NetworkManager",
                       QDBusConnection::systemBus());
    if (!nm.isValid())
        return result;

    QDBusReply<QList<QDBusObjectPath>> reply = nm.call("GetAllDevices");
    if (!reply.isValid())
        return result;

    for (const QDBusObjectPath &devPath : reply.value()) {
        QDBusInterface dev("org.freedesktop.NetworkManager",
                          devPath.path(),
                          "org.freedesktop.DBus.Properties",
                          QDBusConnection::systemBus());
        if (!dev.isValid())
            continue;

        QDBusReply<QVariant> ifaceReply = dev.call("Get", "org.freedesktop.NetworkManager.Device", "Interface");
        QDBusReply<QVariant> typeReply = dev.call("Get", "org.freedesktop.NetworkManager.Device", "DeviceType");
        QString ifaceName = ifaceReply.value().toString();
        quint32 devType = typeReply.value().toUInt();

        quint32 metric = 10000;
        switch (devType) {
        case 1:  metric = 100; break;  // NM_DEVICE_TYPE_ETHERNET
        case 2:  metric = 600; break;  // NM_DEVICE_TYPE_WIFI
        case 14: metric = 50;  break;  // NM_DEVICE_TYPE_WIREGUARD
        case 13: metric = 300; break;  // NM_DEVICE_TYPE_BOND
        case 15: metric = 350; break;  // NM_DEVICE_TYPE_TEAM
        case 16: metric = 400; break;  // NM_DEVICE_TYPE_VLAN
        case 17: metric = 425; break;  // NM_DEVICE_TYPE_BRIDGE
        case 20: metric = 500; break;  // NM_DEVICE_TYPE_VXLAN
        case 22: metric = 460; break;  // NM_DEVICE_TYPE_PPP
        case 23: metric = 550; break;  // NM_DEVICE_TYPE_DUMMY
        case 8:  metric = 700; break;  // NM_DEVICE_TYPE_MODEM
        case 9:  metric = 750; break;  // NM_DEVICE_TYPE_BT
        default: metric = 950; break;  // generic
        }
        result[ifaceName] = metric;
    }
    return result;
}

static QString connectionTypeToInterfaceName(const QMap<QString, QVariantMap> &allSettings)
{
    QString connType;
    if (allSettings.contains("connection"))
        connType = allSettings["connection"].value("type").toString();

    if (connType == "802-3-ethernet")
        return allSettings["802-3-ethernet"].value("interface-name").toString();
    if (connType == "802-11-wireless")
        return allSettings["802-11-wireless"].value("interface-name").toString();
    return allSettings["connection"].value("interface-name").toString();
}

bool NetworkCheckerProxy::setPrimaryInterface(const QString &interfacename)
{
    QDBusInterface nm("org.freedesktop.NetworkManager",
                       "/org/freedesktop/NetworkManager",
                       "org.freedesktop.NetworkManager",
                       QDBusConnection::systemBus());
    if (!nm.isValid()) {
        qWarning() << "NetworkManager DBus not available";
        return false;
    }

    auto activeConns = getActiveConnections();
    if (!activeConns.contains(interfacename)) {
        qWarning() << "Interface not active:" << interfacename;
        return false;
    }

    auto deviceMetrics = getDeviceDefaultMetrics();

    QDBusReply<QList<QDBusObjectPath>> connectionsReply = nm.call("GetConnections");
    if (!connectionsReply.isValid())
        return false;

    for (const QDBusObjectPath &connPath : connectionsReply.value()) {
        QDBusInterface settings("org.freedesktop.NetworkManager",
                                connPath.path(),
                                "org.freedesktop.NetworkManager.Settings.Connection",
                                QDBusConnection::systemBus());
        if (!settings.isValid())
            continue;

        QDBusReply<QMap<QString, QVariantMap>> reply = settings.call("GetSettings");
        if (!reply.isValid())
            continue;

        QMap<QString, QVariantMap> allSettings = reply.value();
        QString ifaceName = connectionTypeToInterfaceName(allSettings);
        if (ifaceName.isEmpty())
            continue;

        bool isTarget = (ifaceName == interfacename);
        quint32 metric = isTarget ? deviceMetrics.value(ifaceName, 100) : deviceMetrics.value(ifaceName, 950);

        if (!isTarget && !activeConns.contains(ifaceName))
            continue;

        if (allSettings.contains("ipv4")) {
            QVariant ipv4Var = allSettings["ipv4"];
            QVariantMap ipv4 = ipv4Var.value<QVariantMap>();
            ipv4["route-metric"] = metric;
            allSettings["ipv4"] = ipv4;
        }

        if (allSettings.contains("ipv6")) {
            QVariant ipv6Var = allSettings["ipv6"];
            QVariantMap ipv6 = ipv6Var.value<QVariantMap>();
            ipv6["route-metric"] = metric;
            allSettings["ipv6"] = ipv6;
        }

        QDBusReply<void> updateReply = settings.call("Update", QVariant::fromValue(allSettings));
        if (!updateReply.isValid()) {
            qWarning() << "Failed to update connection" << connPath.path()
                       << updateReply.error().message();
        }
    }

    return true;
}
