#ifndef NETWORKCHECKERADAPTOR_H
#define NETWORKCHECKERADAPTOR_H

#include <QtCore/QObject>
#include <QtDBus/QtDBus>

class NetworkCheckerProxy;

class NetworkCheckerAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.deepin.demo.NetworkChecker")
    Q_CLASSINFO("D-Bus Introspection", ""
        "  <interface name=\"org.deepin.demo.NetworkChecker\">\n"
        "    <method name=\"isOnline\">\n"
        "      <arg direction=\"in\" type=\"s\" name=\"interfacename\"/>\n"
        "      <arg direction=\"in\" type=\"b\" name=\"byHostname\"/>\n"
        "      <arg direction=\"out\" type=\"b\" name=\"online\"/>\n"
        "    </method>\n"
        "    <method name=\"setPrimaryInterface\">\n"
        "      <arg direction=\"in\" type=\"s\" name=\"interfacename\"/>\n"
        "      <arg direction=\"out\" type=\"b\" name=\"success\"/>\n"
        "    </method>\n"
        "  </interface>\n"
        "")

public:
    explicit NetworkCheckerAdaptor(NetworkCheckerProxy *parent);
    virtual ~NetworkCheckerAdaptor();

public Q_SLOTS:
    bool isOnline(const QString &interfacename, bool byHostname);
    bool setPrimaryInterface(const QString &interfacename);

private:
    NetworkCheckerProxy *m_checkerProxy;
};

#endif
