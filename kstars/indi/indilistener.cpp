/*  INDI Listener
    Copyright (C) 2012 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    Handle INDI Standard properties.
 */

#include "indilistener.h"

#include "clientmanager.h"
#include "deviceinfo.h"
#include "indicap.h"
#include "indiccd.h"
#include "indidome.h"
#include "indifilter.h"
#include "indifocuser.h"
#include "indilightbox.h"
#include "inditelescope.h"
#include "indiweather.h"
#include "kstars.h"
#include "Options.h"

#include "auxiliary/ksnotification.h"

#include <knotification.h>

#include <basedevice.h>
#include <indi_debug.h>

#define NINDI_STD 35

/* INDI standard property used across all clients to enable interoperability. */
const char *indi_std[NINDI_STD] = { "CONNECTION",
                                    "DEVICE_PORT",
                                    "TIME_UTC",
                                    "TIME_LST",
                                    "GEOGRAPHIC_COORD",
                                    "EQUATORIAL_COORD",
                                    "EQUATORIAL_EOD_COORD",
                                    "EQUATORIAL_EOD_COORD_REQUEST",
                                    "HORIZONTAL_COORD",
                                    "TELESCOPE_ABORT_MOTION",
                                    "ON_COORD_SET",
                                    "SOLAR_SYSTEM",
                                    "TELESCOPE_MOTION_NS",
                                    "TELESCOPE_MOTION_WE",
                                    "TELESCOPE_PARK",
                                    "DOME_PARK",
                                    "GPS_REFRESH",
                                    "WEATHER_STATUS",
                                    "CCD_EXPOSURE",
                                    "CCD_TEMPERATURE",
                                    "CCD_FRAME",
                                    "CCD_FRAME_TYPE",
                                    "CCD_BINNING",
                                    "CCD_INFO",
                                    "CCD_VIDEO_STREAM",
                                    "RAW_STREAM",
                                    "IMAGE_STREAM",
                                    "FOCUS_SPEED",
                                    "FOCUS_MOTION",
                                    "FOCUS_TIMER",
                                    "FILTER_SLOT",
                                    "WATCHDOG_HEARTBEAT",
                                    "CAP_PARK",
                                    "FLAT_LIGHT_CONTROL",
                                    "FLAT_LIGHT_INTENSITY" };

INDIListener *INDIListener::_INDIListener = nullptr;

INDIListener *INDIListener::Instance()
{
    if (_INDIListener == nullptr)
    {
        _INDIListener = new INDIListener(KStars::Instance());

        connect(_INDIListener, &INDIListener::newTelescope, [&]()
        {
            KStars::Instance()->slotSetTelescopeEnabled(true);
        });

        connect(_INDIListener, &INDIListener::newDome, [&]()
        {
            KStars::Instance()->slotSetDomeEnabled(true);
        });

    }

    return _INDIListener;
}

INDIListener::INDIListener(QObject *parent) : QObject(parent)
{
}

INDIListener::~INDIListener()
{
    qDeleteAll(devices);
    qDeleteAll(st4Devices);
}

bool INDIListener::isStandardProperty(const QString &name)
{
    for (auto &item : indi_std)
    {
        if (!strcmp(name.toLatin1().constData(), item))
            return true;
    }
    return false;
}

ISD::GDInterface *INDIListener::getDevice(const QString &name)
{
    foreach (ISD::GDInterface *gi, devices)
    {
        if (!strcmp(gi->getDeviceName(), name.toLatin1().constData()))
            return gi;
    }
    return nullptr;
}

void INDIListener::addClient(ClientManager *cm)
{
    qCDebug(KSTARS_INDI) << "INDIListener: Adding a new client manager to INDI listener..";

    Qt::ConnectionType type = Qt::BlockingQueuedConnection;

#ifdef USE_QT5_INDI
    type = Qt::DirectConnection;
#endif

    clients.append(cm);

    connect(cm, SIGNAL(newINDIDevice(DeviceInfo*)), this, SLOT(processDevice(DeviceInfo*)), type);
    connect(cm, SIGNAL(removeINDIDevice(DeviceInfo*)), this, SLOT(removeDevice(DeviceInfo*)), type);
    connect(cm, SIGNAL(newINDIProperty(INDI::Property*)), this, SLOT(registerProperty(INDI::Property*)), type);
    connect(cm, SIGNAL(removeINDIProperty(INDI::Property*)), this, SLOT(removeProperty(INDI::Property*)), type);

    connect(cm, SIGNAL(newINDISwitch(ISwitchVectorProperty*)), this, SLOT(processSwitch(ISwitchVectorProperty*)));
    connect(cm, SIGNAL(newINDIText(ITextVectorProperty*)), this, SLOT(processText(ITextVectorProperty*)));
    connect(cm, SIGNAL(newINDINumber(INumberVectorProperty*)), this, SLOT(processNumber(INumberVectorProperty*)));
    connect(cm, SIGNAL(newINDILight(ILightVectorProperty*)), this, SLOT(processLight(ILightVectorProperty*)));
    connect(cm, SIGNAL(newINDIBLOB(IBLOB*)), this, SLOT(processBLOB(IBLOB*)));
    #if INDI_VERSION_MAJOR >= 1 && INDI_VERSION_MINOR >= 5
    connect(cm, SIGNAL(newINDIUniversalMessage(QString)), this, SLOT(processUniversalMessage(QString)));
    #endif
}

void INDIListener::removeClient(ClientManager *cm)
{
    qCDebug(KSTARS_INDI) << "INDIListener: Removing client manager for server" << cm->getHost() << "@" << cm->getPort();

    QList<ISD::GDInterface *>::iterator it = devices.begin();
    clients.removeOne(cm);

    while (it != devices.end())
    {
        DriverInfo *dv  = (*it)->getDriverInfo();
        bool hostSource = (dv->getDriverSource() == HOST_SOURCE) || (dv->getDriverSource() == GENERATED_SOURCE);

        if (cm->isDriverManaged(dv))
        {
            // If we have multiple devices per driver, we need to remove them all
            if (dv->getAuxInfo().value("mdpd", false).toBool() == true)
            {
                while (it != devices.end())
                {
                    if (dv->getDevice((*it)->getDeviceName()) != nullptr)
                    {
                        it = devices.erase(it);
                    }
                    else
                        break;
                }
            }
            else
                it = devices.erase(it);

            cm->removeManagedDriver(dv);
            cm->disconnect(this);
            if (hostSource)
                return;
        }
        else
            ++it;
    }
}

void INDIListener::processDevice(DeviceInfo *dv)
{
    qCDebug(KSTARS_INDI) << "INDIListener: New device" << dv->getBaseDevice()->getDeviceName();

    ISD::GDInterface *gd = new ISD::GenericDevice(*dv);

    devices.append(gd);

    emit newDevice(gd);
}

void INDIListener::removeDevice(DeviceInfo *dv)
{
    qCDebug(KSTARS_INDI) << "INDIListener: Removing device" << dv->getBaseDevice()->getDeviceName() << "with unique label "
                 << dv->getDriverInfo()->getUniqueLabel();

    foreach (ISD::GDInterface *gd, devices)
    {
        if (gd->getDeviceInfo() == dv)
        {
            emit deviceRemoved(gd);
            devices.removeOne(gd);
            delete (gd);
        }
    }

    /*foreach(ISD::GDInterface *gd, devices)
    {
        if ( (dv->getDriverInfo()->getDevices().size() > 1 && gd->getDeviceName() == dv->getBaseDevice()->getDeviceName())
            || dv->getDriverInfo()->getUniqueLabel() == gd->getDeviceName() || dv->getDriverInfo()->getDriverSource() == HOST_SOURCE
                || dv->getDriverInfo()->getDriverSource() == GENERATED_SOURCE)
        {
            if (dv->getDriverInfo()->getUniqueLabel().contains("Query") == false)
                emit deviceRemoved(gd);
            devices.removeOne(gd);
            delete(gd);

            if (dv->getDriverInfo()->getDriverSource() != HOST_SOURCE && dv->getDriverInfo()->getDriverSource() != GENERATED_SOURCE)
                return;
        }
    }*/
}

void INDIListener::registerProperty(INDI::Property *prop)
{
   qCDebug(KSTARS_INDI) << "<" << prop->getDeviceName() << ">: <" << prop->getName() << ">";

    foreach (ISD::GDInterface *gd, devices)
    {
        if (!strcmp(gd->getDeviceName(), prop->getDeviceName()))
        {
            if (!strcmp(prop->getName(), "EQUATORIAL_EOD_COORD") ||
                !strcmp(prop->getName(), "EQUATORIAL_COORD") ||
                !strcmp(prop->getName(), "HORIZONTAL_COORD"))
            {
                if (gd->getType() == KSTARS_UNKNOWN)
                {
                    devices.removeOne(gd);
                    gd = new ISD::Telescope(gd);
                    devices.append(gd);
                }

                emit newTelescope(gd);
            }
            else if (!strcmp(prop->getName(), "CCD_EXPOSURE"))
            {
                if (gd->getType() == KSTARS_UNKNOWN)
                {
                    devices.removeOne(gd);
                    gd = new ISD::CCD(gd);
                    devices.append(gd);
                }

                emit newCCD(gd);
            }
            else if (!strcmp(prop->getName(), "FILTER_SLOT"))
            {
                if (gd->getType() == KSTARS_UNKNOWN)
                {
                    devices.removeOne(gd);
                    gd = new ISD::Filter(gd);
                    devices.append(gd);
                }

                emit newFilter(gd);
            }
            else if (!strcmp(prop->getName(), "FOCUS_MOTION"))
            {
                if (gd->getType() == KSTARS_UNKNOWN)
                {
                    devices.removeOne(gd);
                    gd = new ISD::Focuser(gd);
                    devices.append(gd);
                }

                emit newFocuser(gd);
            }

            else if (!strcmp(prop->getName(), "DOME_MOTION"))
            {
                if (gd->getType() == KSTARS_UNKNOWN)
                {
                    devices.removeOne(gd);
                    gd = new ISD::Dome(gd);
                    devices.append(gd);
                }

                emit newDome(gd);
            }
            else if (!strcmp(prop->getName(), "WEATHER_STATUS"))
            {
                if (gd->getType() == KSTARS_UNKNOWN)
                {
                    devices.removeOne(gd);
                    gd = new ISD::Weather(gd);
                    devices.append(gd);
                }

                emit newWeather(gd);
            }
            else if (!strcmp(prop->getName(), "CAP_PARK"))
            {
                if (gd->getType() == KSTARS_UNKNOWN)
                {
                    devices.removeOne(gd);
                    gd = new ISD::DustCap(gd);
                    devices.append(gd);
                }

                emit newDustCap(gd);
            }
            else if (!strcmp(prop->getName(), "FLAT_LIGHT_CONTROL"))
            {
                // If light box part of dust cap
                if (gd->getType() == KSTARS_UNKNOWN)
                {
                    if (gd->getBaseDevice()->getDriverInterface() & INDI::BaseDevice::DUSTCAP_INTERFACE)
                    {
                        devices.removeOne(gd);
                        gd = new ISD::DustCap(gd);
                        devices.append(gd);

                        emit newDustCap(gd);
                    }
                    // If stand-alone light box
                    else
                    {
                        devices.removeOne(gd);
                        gd = new ISD::LightBox(gd);
                        devices.append(gd);

                        emit newLightBox(gd);
                    }
                }
            }

            if (!strcmp(prop->getName(), "TELESCOPE_TIMED_GUIDE_WE"))
            {
                ISD::ST4 *st4Driver = new ISD::ST4(gd->getBaseDevice(), gd->getDriverInfo()->getClientManager());
                st4Devices.append(st4Driver);
                emit newST4(st4Driver);
            }

            gd->registerProperty(prop);
            break;
        }
    }
}

void INDIListener::removeProperty(INDI::Property *prop)
{
    if (prop == nullptr)
        return;

    foreach (ISD::GDInterface *gd, devices)
    {
        if (!strcmp(gd->getDeviceName(), prop->getDeviceName()))
        {
            gd->removeProperty(prop);
            return;
        }
    }
}

void INDIListener::processSwitch(ISwitchVectorProperty *svp)
{
    foreach (ISD::GDInterface *gd, devices)
    {
        if (!strcmp(gd->getDeviceName(), svp->device))
        {
            gd->processSwitch(svp);
            break;
        }
    }
}

void INDIListener::processNumber(INumberVectorProperty *nvp)
{
    foreach (ISD::GDInterface *gd, devices)
    {
        if (!strcmp(gd->getDeviceName(), nvp->device))
        {
            gd->processNumber(nvp);
            break;
        }
    }
}

void INDIListener::processText(ITextVectorProperty *tvp)
{
    foreach (ISD::GDInterface *gd, devices)
    {
        if (!strcmp(gd->getDeviceName(), tvp->device))
        {
            gd->processText(tvp);
            break;
        }
    }
}

void INDIListener::processLight(ILightVectorProperty *lvp)
{
    foreach (ISD::GDInterface *gd, devices)
    {
        if (!strcmp(gd->getDeviceName(), lvp->device))
        {
            gd->processLight(lvp);
            break;
        }
    }
}

void INDIListener::processBLOB(IBLOB *bp)
{
    foreach (ISD::GDInterface *gd, devices)
    {
        if (!strcmp(gd->getDeviceName(), bp->bvp->device))
        {
            gd->processBLOB(bp);
            break;
        }
    }
}

void INDIListener::processMessage(INDI::BaseDevice *dp, int messageID)
{
    foreach (ISD::GDInterface *gd, devices)
    {
        if (!strcmp(gd->getDeviceName(), dp->getDeviceName()))
        {
            gd->processMessage(messageID);
            break;
        }
    }
}

void INDIListener::processUniversalMessage(const QString &message)
{
    QString displayMessage = message;
    // Remove timestamp info as it is not suitable for message box
    int colonIndex = displayMessage.indexOf(": ");
    if (colonIndex > 0)
        displayMessage = displayMessage.mid(colonIndex+2);

    // Special case for Alignment since it is not tied to a device
    if (displayMessage.startsWith("[ALIGNMENT]"))
    {
        qCDebug(KSTARS_INDI) << "AlignmentSubSystem:" << displayMessage;
        return;
    }

    if (Options::messageNotificationINDI())
    {
        KNotification::event(QLatin1String("IndiServerMessage"),
                             displayMessage+" (INDI)");
    }
    else
    {
        KSNotification::transient(displayMessage, i18n("INDI Server Message"));
    }
}
