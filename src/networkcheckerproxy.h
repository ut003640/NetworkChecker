#ifndef NETWORKCHECKERPROXY_H
#define NETWORKCHECKERPROXY_H

#include <QObject>

#include <QSharedPointer>

class NetworkChecker;

class NetworkCheckerProxy : public QObject
{
    Q_OBJECT
public:
    explicit NetworkCheckerProxy(QObject *parent = nullptr);

    bool isOnline(const QString &interfacename, bool byHostname);

private:
    QSharedPointer<NetworkChecker> m_checker;
};

#endif
