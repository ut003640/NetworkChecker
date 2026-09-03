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
#include <NetworkManagerQt/Device>

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

bool NetworkCheckerProxy::setPrimaryInterface(const QString &interfacename)
{
    const NetworkManager::Device::List &allDevices = NetworkManager::networkInterfaces();
    const quint32 primaryMetric = 100;
    const quint32 secondaryMetric = 1000;
    bool targetFound = false;
    QMap<QString, NMVariantMapMap> deviceSettings;
    for (const NetworkManager::Device::Ptr &dev : allDevices) {
        // 跳过没有活跃连接的网卡
        if (dev->activeConnection().isNull())
            continue;

        auto ac = dev->activeConnection();
        if (!ac || ac->state() != NetworkManager::ActiveConnection::Activated)
            continue;

        bool isTarget = (dev->interfaceName() == interfacename);
        if (isTarget)
            targetFound = true;

        quint32 metric = isTarget ? primaryMetric : secondaryMetric;

        NetworkManager::Connection::Ptr conn = ac->connection();
        NetworkManager::ConnectionSettings::Ptr settings = conn->settings();

        bool needReapply = false;
        NetworkManager::Ipv4Setting::Ptr ipv4 = settings->setting(NetworkManager::Setting::Ipv4).staticCast<NetworkManager::Ipv4Setting>();
        if (ipv4) {
            ipv4->setRouteMetric(metric);
            ipv4->setInitialized(true);
            needReapply = true;
        }

        NetworkManager::Ipv6Setting::Ptr ipv6 = settings->setting(NetworkManager::Setting::Ipv6).staticCast<NetworkManager::Ipv6Setting>();
        if (ipv6) {
            ipv6->setRouteMetric(metric);
            ipv6->setInitialized(true);
            needReapply = true;
        }

        if (needReapply) {
            deviceSettings[dev->uni()] = settings->toMap();
        }
    }

    if (!targetFound) {
        qWarning() << "target interface not found or not active:" << interfacename;
        return false;
    }

    if (deviceSettings.isEmpty()) {
        qWarning() << "can't find device for change route";
        return false;
    }

    // 通过 D-Bus 调用 Reapply 使修改的路由 metric 生效
    // Reapply 签名: (a{sa{sv}}tu) — settings_map, version_id(0), flags(0)
    for (auto it = deviceSettings.constBegin(); it != deviceSettings.constEnd(); ++it) {
        const QString &path = it.key();
        const NMVariantMapMap &settingMap = it.value();
        QDBusInterface dbusIface("org.freedesktop.NetworkManager", path, "org.freedesktop.NetworkManager.Device", QDBusConnection::systemBus());
        QDBusMessage reply = dbusIface.call("Reapply", QVariant::fromValue(settingMap), QVariant::fromValue(static_cast<quint64>(0)), QVariant::fromValue(static_cast<quint32>(0)));
        if (reply.type() == QDBusMessage::ErrorMessage) {
            qWarning() << "Reapply failed for device" << path << "error:" << reply.errorMessage();
            return false;
        }
    }

    return true;
}
