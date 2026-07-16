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

protected:
    virtual bool checkIfInterfaceOnline(const QString &interfacename) = 0;

private:
    bool checkInterfaceExists(const QString &interfacename);
};

#endif
