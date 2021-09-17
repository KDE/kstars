/*
    SPDX-FileCopyrightText: 2014 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "indidbus.h"

#include "indiadaptor.h"
#include "nan.h"
#include "indi/drivermanager.h"
#include "indi/servermanager.h"
#include "indi/driverinfo.h"
#include "indi/clientmanager.h"
#include "indi/indilistener.h"
#include "indi/deviceinfo.h"

#include "kstars_debug.h"

#include <basedevice.h>

INDIDBus::INDIDBus(QObject *parent) : QObject(parent)
{
    new INDIAdaptor(this);
    QDBusConnection::sessionBus().registerObject("/KStars/INDI", this);
}

bool INDIDBus::start(const QString &port, const QStringList &drivers)
{
    QList<DriverInfo *> newDrivers;

    for (auto &driver : drivers)
    {
        DriverInfo *drv = DriverManager::Instance()->findDriverByExec(driver);

        if (drv == nullptr)
            continue;

        DriverInfo *di = new DriverInfo(QString("%1").arg(driver));
        di->setHostParameters("localhost", port.isEmpty() ? "7624" : port);
        di->setDriverSource(EM_XML);
        di->setExecutable(driver);
        di->setUniqueLabel(drv->getUniqueLabel().isEmpty() ? drv->getLabel() : drv->getUniqueLabel());

        DriverManager::Instance()->addDriver(di);
        newDrivers.append(di);
    }

    return DriverManager::Instance()->startDevices(newDrivers);
}

bool INDIDBus::stop(const QString &port)
{
    QList<DriverInfo *> stopDrivers;

    foreach (DriverInfo *di, DriverManager::Instance()->getDrivers())
    {
        if (di->getClientState() && di->getPort() == port)
            stopDrivers.append(di);
    }

    if (stopDrivers.isEmpty())
    {
        qCWarning(KSTARS) << "Could not find devices on port " << port;
        return false;
    }

    DriverManager::Instance()->stopDevices(stopDrivers);

    foreach (DriverInfo *di, stopDrivers)
        DriverManager::Instance()->removeDriver(di);

    qDeleteAll(stopDrivers);

    return true;
}

bool INDIDBus::connect(const QString &host, const QString &port)
{
    DriverInfo *remote_indi = new DriverInfo(QString("INDI Remote Host"));

    remote_indi->setHostParameters(host, port);

    remote_indi->setDriverSource(GENERATED_SOURCE);

    if (DriverManager::Instance()->connectRemoteHost(remote_indi) == false)
    {
        delete (remote_indi);
        remote_indi = 0;
        return false;
    }

    DriverManager::Instance()->addDriver(remote_indi);

    return true;
}

bool INDIDBus::disconnect(const QString &host, const QString &port)
{
    foreach (DriverInfo *di, DriverManager::Instance()->getDrivers())
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

QStringList INDIDBus::getDevices()
{
    QStringList devices;

    foreach (ISD::GDInterface *gd, INDIListener::Instance()->getDevices())
    {
        INDI::BaseDevice *dp = gd->getBaseDevice();
        if (!dp)
            continue;

        devices << dp->getDeviceName();
    }

    return devices;
}

QStringList INDIDBus::getProperties(const QString &device)
{
    QStringList properties;

    foreach (ISD::GDInterface *gd, INDIListener::Instance()->getDevices())
    {
        INDI::BaseDevice *dp = gd->getBaseDevice();
        if (!dp)
            continue;

        if (dp->getDeviceName() == device)
        {
            // Let's print a list of all device properties
            for (const auto &prop : *dp->getProperties())
            {
                switch (prop->getType())
                {
                    case INDI_SWITCH:
                        for (const auto &it : *prop->getSwitch())
                            properties << device + '.' + QString(prop->getName()) + '.' + it.getName();
                        break;

                    case INDI_TEXT:
                        for (const auto &it : *prop->getText())
                            properties << device + '.' + QString(prop->getName()) + '.' + it.getName();
                        break;

                    case INDI_NUMBER:
                        for (const auto &it : *prop->getNumber())
                            properties << device + '.' + QString(prop->getName()) + '.' + it.getName();
                        break;

                    case INDI_LIGHT:
                        for (const auto &it : *prop->getLight())
                            properties << device + '.' + QString(prop->getName()) + '.' + it.getName();
                        break;

                    case INDI_BLOB:
                        for (const auto &it : *prop->getBLOB())
                            properties << device + '.' + QString(prop->getName()) + '.' + it.getName();
                        break;

                    case INDI_UNKNOWN:
                        qCWarning(KSTARS) << device << '.' << QString(prop->getName()) << " has an unknown type! Aborting...";
                        return properties;
                        break;
                }
            }

            return properties;
        }
    }

    qCWarning(KSTARS) << "Could not find device: " << device;
    return properties;
}

QString INDIDBus::getPropertyState(const QString &device, const QString &property)
{
    QString status = "Invalid";

    foreach (ISD::GDInterface *gd, INDIListener::Instance()->getDevices())
    {
        INDI::BaseDevice *dp = gd->getBaseDevice();
        if (!dp)
            continue;

        if (dp->getDeviceName() == device)
        {
            auto prop = dp->getProperty(property.toLatin1());
            if (prop)
            {
                status = QString(pstateStr(prop->getState()));
                return status;
            }

            qCWarning(KSTARS) << "Could not find property: " << device << '.' << property;
            return status;
        }
    }

    qCWarning(KSTARS) << "Could not find property: " << device << '.' << property;
    return status;
}

bool INDIDBus::sendProperty(const QString &device, const QString &property)
{
    foreach (ISD::GDInterface *gd, INDIListener::Instance()->getDevices())
    {
        INDI::BaseDevice *dp = gd->getBaseDevice();
        if (!dp)
            continue;

        ClientManager *cm = gd->getDeviceInfo()->getDriverInfo()->getClientManager();

        if (dp->getDeviceName() == device)
        {
            auto prop = dp->getProperty(property.toLatin1());
            if (prop)
            {
                switch (prop->getType())
                {
                    case INDI_SWITCH:
                        prop->getSwitch()->setState(IPS_BUSY);
                        cm->sendNewSwitch(prop->getSwitch());
                        break;

                    case INDI_TEXT:
                        prop->getText()->setState(IPS_BUSY);
                        cm->sendNewText(prop->getText());
                        break;

                    case INDI_NUMBER:
                        prop->getNumber()->setState(IPS_BUSY);
                        cm->sendNewNumber(prop->getNumber());
                        break;

                    default:
                        qCWarning(KSTARS) << "Only switch, text, and number properties are supported.";
                        return false;
                }

                return true;
            }

            qCWarning(KSTARS) << "Could not find property: " << device << '.' << property;
            return false;
        }
    }

    qCWarning(KSTARS) << "Could not find property: " << device << '.' << property;
    return false;
}

QString INDIDBus::getLight(const QString &device, const QString &property, const QString &lightName)
{
    QString status = "Invalid";

    foreach (ISD::GDInterface *gd, INDIListener::Instance()->getDevices())
    {
        INDI::BaseDevice *dp = gd->getBaseDevice();
        if (!dp)
            continue;

        if (dp->getDeviceName() == device)
        {
            auto prop = dp->getProperty(property.toLatin1());
            if (prop)
            {
                auto lp = prop->getLight();
                if (lp)
                {
                    auto l = lp->findWidgetByName(lightName.toLatin1());
                    if (l)
                    {
                        status = QString(l->getStateAsString());
                        return status;
                    }
                }
            }

            qCWarning(KSTARS) << "Could not find property: " << device << '.' << property;
            return status;
        }
    }

    qCWarning(KSTARS) << "Could not find property: " << device << '.' << property;
    return status;
}

bool INDIDBus::setSwitch(const QString &device, const QString &property, const QString &switchName,
                         const QString &status)
{
    if (status != "On" && status != "Off")
    {
        qCWarning(KSTARS) << "Invalid switch status requested: " << status;
        return false;
    }

    foreach (ISD::GDInterface *gd, INDIListener::Instance()->getDevices())
    {
        INDI::BaseDevice *dp = gd->getBaseDevice();
        if (!dp)
            continue;

        if (dp->getDeviceName() == device)
        {
            auto sp = dp->getSwitch(property.toLatin1());

            if (sp->getRule() == ISR_1OFMANY && status == "Off")
            {
                qCWarning(KSTARS) << "Cannot set ISR_1OFMANY switch to Off. At least one switch must be On.";
                return false;
            }

            if (sp->getRule() == ISR_1OFMANY || sp->getRule() == ISR_ATMOST1)
                sp->reset();

            auto sw = sp->findWidgetByName(switchName.toLatin1());

            if (sw)
            {
                sw->setState(status == "On" ? ISS_ON : ISS_OFF);
                return true;
            }

            qCWarning(KSTARS) << "Could not find property: " << device << '.' << property << '.' << switchName;
            return false;
        }
    }

    qCWarning(KSTARS) << "Could not find property: " << device << '.' << property << '.' << switchName;
    return false;
}

QString INDIDBus::getSwitch(const QString &device, const QString &property, const QString &switchName)
{
    QString result("Invalid");

    foreach (ISD::GDInterface *gd, INDIListener::Instance()->getDevices())
    {
        INDI::BaseDevice *dp = gd->getBaseDevice();
        if (!dp)
            continue;

        if (dp->getDeviceName() == device)
        {
            auto sp = dp->getSwitch(property.toLatin1());
            if (sp)
            {
                auto sw = sp->findWidgetByName(switchName.toLatin1());
                if (sw)
                {
                    result = ((sw->getState() == ISS_ON) ? "On" : "Off");
                    return result;
                }

                qCWarning(KSTARS) << "Could not find property: " << device << '.' << property << '.' << switchName;
                return result;
            }

            qCWarning(KSTARS) << "Could not find property: " << device << '.' << property << '.' << switchName;
            return result;
        }
    }

    qCWarning(KSTARS) << "Could not find property: " << device << '.' << property << '.' << switchName;
    return result;
}

bool INDIDBus::setText(const QString &device, const QString &property, const QString &textName, const QString &text)
{
    foreach (ISD::GDInterface *gd, INDIListener::Instance()->getDevices())
    {
        INDI::BaseDevice *dp = gd->getBaseDevice();
        if (!dp)
            continue;

        if (dp->getDeviceName() == device)
        {
            auto tp = dp->getText(property.toLatin1());

            if (tp)
            {
                auto t = tp->findWidgetByName(textName.toLatin1());
                if (t)
                {
                    t->setText(text.toLatin1());
                    return true;
                }

                qCWarning(KSTARS) << "Could not find property: " << device << '.' << property << '.' << textName;
                return false;
            }

            qCWarning(KSTARS) << "Could not find property: " << device << '.' << property << '.' << textName;
            return false;
        }
    }

    qCWarning(KSTARS) << "Could not find property: " << device << '.' << property << '.' << textName;
    return false;
}

QString INDIDBus::getText(const QString &device, const QString &property, const QString &textName)
{
    QString result("Invalid");

    foreach (ISD::GDInterface *gd, INDIListener::Instance()->getDevices())
    {
        INDI::BaseDevice *dp = gd->getBaseDevice();
        if (!dp)
            continue;

        if (dp->getDeviceName() == device)
        {
            auto tp = dp->getText(property.toLatin1());
            if (tp)
            {
                auto t = tp->findWidgetByName(textName.toLatin1());
                if (t)
                {
                    result = QString(t->getText());
                    return result;
                }

                qCWarning(KSTARS) << "Could not find property: " << device << '.' << property << '.' << textName;
                return result;
            }

            qCWarning(KSTARS) << "Could not find property: " << device << '.' << property << '.' << textName;
            return result;
        }
    }

    qCWarning(KSTARS) << "Could not find property: " << device << '.' << property << '.' << textName;
    return result;
}

bool INDIDBus::setNumber(const QString &device, const QString &property, const QString &numberName, double value)
{
    foreach (ISD::GDInterface *gd, INDIListener::Instance()->getDevices())
    {
        INDI::BaseDevice *dp = gd->getBaseDevice();
        if (!dp)
            continue;

        if (dp->getDeviceName() == device)
        {
            auto np = dp->getNumber(property.toLatin1());

            if (np)
            {
                auto n = np->findWidgetByName(numberName.toLatin1());
                if (n)
                {
                    n->setValue(value);
                    return true;
                }

                qCWarning(KSTARS) << "Could not find property: " << device << '.' << property << '.' << numberName;
                return false;
            }

            qCWarning(KSTARS) << "Could not find property: " << device << '.' << property << '.' << numberName;
            return false;
        }
    }

    qCWarning(KSTARS) << "Could not find property: " << device << '.' << property << '.' << numberName;
    return false;
}

double INDIDBus::getNumber(const QString &device, const QString &property, const QString &numberName)
{
    double result = NaN::d;

    foreach (ISD::GDInterface *gd, INDIListener::Instance()->getDevices())
    {
        INDI::BaseDevice *dp = gd->getBaseDevice();
        if (!dp)
            continue;

        if (dp->getDeviceName() == device)
        {
            auto np = dp->getNumber(property.toLatin1());
            if (np)
            {
                auto n = np->findWidgetByName(numberName.toLatin1());
                if (n)
                {
                    result = n->getValue();
                    return result;
                }

                qCWarning(KSTARS) << "Could not find property: " << device << '.' << property << '.' << numberName;
                return result;
            }

            qCWarning(KSTARS) << "Could not find property: " << device << '.' << property << '.' << numberName;
            return result;
        }
    }

    qCWarning(KSTARS) << "Could not find property: " << device << '.' << property << '.' << numberName;
    return result;
}

QByteArray INDIDBus::getBLOBData(const QString &device, const QString &property, const QString &blobName,
                                 QString &blobFormat, int &size)
{
    QByteArray array;
    size = -1;

    foreach (ISD::GDInterface *gd, INDIListener::Instance()->getDevices())
    {
        INDI::BaseDevice *dp = gd->getBaseDevice();
        if (!dp)
            continue;

        if (dp->getDeviceName() == device)
        {
            auto bp = dp->getBLOB(property.toLatin1());
            if (bp)
            {
                auto b = bp->findWidgetByName(blobName.toLatin1());
                if (b)
                {
                    const char *rawData = static_cast<const char *>(b->getBlob());
                    array               = QByteArray::fromRawData(rawData, b->getSize());
                    size                = b->getBlobLen();
                    blobFormat          = QString(b->getFormat()).trimmed();

                    return array;
                }

                qCWarning(KSTARS) << "Could not find property: " << device << '.' << property << '.' << blobName;
                return array;
            }

            qCWarning(KSTARS) << "Could not find property: " << device << '.' << property << '.' << blobName;
            return array;
        }
    }

    qCWarning(KSTARS) << "Could not find property: " << device << '.' << property << '.' << blobName;
    return array;
}

QString INDIDBus::getBLOBFile(const QString &device, const QString &property, const QString &blobName,
                              QString &blobFormat, int &size)
{
    QString filename;
    size = -1;

    foreach (ISD::GDInterface *gd, INDIListener::Instance()->getDevices())
    {
        INDI::BaseDevice *dp = gd->getBaseDevice();
        if (!dp)
            continue;

        if (dp->getDeviceName() == device)
        {
            auto bp = dp->getBLOB(property.toLatin1());
            if (bp)
            {
                auto b = bp->findWidgetByName(blobName.toLatin1());
                if (b)
                {
                    filename   = QString(((char *)b->aux2));
                    size       = b->bloblen;
                    blobFormat = QString(b->format).trimmed();

                    return filename;
                }

                qCWarning(KSTARS) << "Could not find property: " << device << '.' << property << '.' << blobName;
                return filename;
            }

            qCWarning(KSTARS) << "Could not find property: " << device << '.' << property << '.' << blobName;
            return filename;
        }
    }

    qCWarning(KSTARS) << "Could not find property: " << device << '.' << property << '.' << blobName;
    return filename;
}
