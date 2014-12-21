/*  INDI DBUS Interface
    Copyright (C) 2014 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "indi/drivermanager.h"
#include "indi/servermanager.h"
#include "indi/driverinfo.h"

#include "indidbus.h"
#include "indiadaptor.h"

INDIDBUS::INDIDBUS(QObject *parent) :
    QObject(parent)
{
    new INDIAdaptor(this);
    QDBusConnection::sessionBus().registerObject("/KStars/INDI",  this);
}

void INDIDBUS::startINDI(const QString &port, const QStringList &drivers)
{
    QList<DriverInfo*> newDrivers;

    foreach(QString driver, drivers)
    {
        DriverInfo *di = new DriverInfo(QString("%1").arg(driver));
        di->setHostParameters("localhost", port.isEmpty() ? "7624" : port);
        di->setDriverSource(HOST_SOURCE);
        di->setDriver(driver);

        DriverManager::Instance()->addDriver(di);
        newDrivers.append(di);

    }

    DriverManager::Instance()->startDevices(newDrivers);
}

void INDIDBUS::stopINDI(const QString &port)
{
    QList<DriverInfo*> stopDrivers;

    foreach(DriverInfo *di, DriverManager::Instance()->getDrivers())
    {
        if (di->getClientState() && di->getPort() == port)
            stopDrivers.append(di);            
    }

    DriverManager::Instance()->stopDevices(stopDrivers);

    foreach(DriverInfo *di, stopDrivers)
        DriverManager::Instance()->removeDriver(di);

    qDeleteAll(stopDrivers);
}


bool INDIDBUS::connectINDI(const QString &host, const QString &port)
{
    DriverInfo *remote_indi = new DriverInfo(QString("INDI Remote Host"));

    remote_indi->setHostParameters(host, port);

    remote_indi->setDriverSource(GENERATED_SOURCE);       

    if (DriverManager::Instance()->connectRemoteHost(remote_indi) == false)
    {
        delete (remote_indi);
        remote_indi=0;
        return false;
    }

    DriverManager::Instance()->addDriver(remote_indi);

    return true;
}

bool INDIDBUS::disconnectINDI(const QString &host, const QString &port)
{

    foreach(DriverInfo *di, DriverManager::Instance()->getDrivers())
    {
        if (di->getHost() == host && di->getPort() == port && di->getDriverSource() == GENERATED_SOURCE)
        {
            if (DriverManager::Instance()->disconnectRemoteHost(di))
            {
                DriverManager::Instance()->removeDriver(di);
                return true;
            }
            else
                return false;
        }
    }

    return false;
}

