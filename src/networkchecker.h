#ifndef NETWORKCHECKER_H
#define NETWORKCHECKER_H

#include <QObject>

class NetworkChecker : public QObject
{
    Q_OBJECT
public:
    explicit NetworkChecker(QObject *parent = nullptr);

public Q_SLOTS:
    bool isOnline(const QString &interfacename);

private:
    bool checkInterfaceExists(const QString &interfacename);
    bool tcpCheckThroughInterface(const QString &interfacename, const QString &host);
};

#endif
