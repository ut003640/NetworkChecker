#ifndef NETWORKCHECKERPROXY_H
#define NETWORKCHECKERPROXY_H

#include <QObject>
#include <QSharedPointer>

#include <QtDBus/QtDBus>

class NetworkChecker;

class NetworkCheckerProxy : public QObject
{
    Q_OBJECT
public:
    explicit NetworkCheckerProxy(QObject *parent = nullptr);

    bool isOnline(const QString &interfacename, bool byHostname);
    bool setPrimaryInterface(const QString &interfacename);

private:
    QSharedPointer<NetworkChecker> m_checker;
};

#endif
