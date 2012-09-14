/*  INDI Listener
    Copyright (C) 2012 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    Handle INDI Standard properties.
 */

#include <libindi/baseclient.h>
#include <libindi/basedevice.h>

#include "indilistener.h"
#include "indicommon.h"
#include "inditelescope.h"
#include "indifocuser.h"
#include "indiccd.h"
#include "indifilter.h"
#include "clientmanager.h"
#include "driverinfo.h"
#include "fitsviewer/fitsviewer.h"

#include "kstars.h"
#include "Options.h"


#include <QDebug>

#include <KMessageBox>
#include <KStatusBar>

#define NINDI_STD	27

/* INDI standard property used across all clients to enable interoperability. */
const char * indi_std[NINDI_STD] =
    {"CONNECTION", "DEVICE_PORT", "TIME_UTC", "TIME_LST", "TIME_UTC_OFFSET" , "GEOGRAPHIC_COORD", "EQUATORIAL_COORD",
     "EQUATORIAL_EOD_COORD", "EQUATORIAL_EOD_COORD_REQUEST", "HORIZONTAL_COORD", "TELESCOPE_ABORT_MOTION", "ON_COORD_SET",
     "SOLAR_SYSTEM", "TELESCOPE_MOTION_NS", "TELESCOPE_MOTION_WE",  "TELESCOPE_PARK", "CCD_EXPOSURE_REQUEST",
     "CCD_TEMPERATURE_REQUEST", "CCD_FRAME", "CCD_FRAME_TYPE", "CCD_BINNING", "CCD_INFO", "VIDEO_STREAM",
     "FOCUS_SPEED", "FOCUS_MOTION", "FOCUS_TIMER", "FILTER_SLOT" };

INDIListener * INDIListener::_INDIListener = NULL;

INDIListener * INDIListener::Instance()
{
    if (_INDIListener == NULL)
        _INDIListener = new INDIListener();

    return _INDIListener;
}

INDIListener::INDIListener()
{
    batchMode = false;
    ISOMode   = true;
    fv        = NULL;
}

bool INDIListener::isStandardProperty(const QString &name)
{
    for (int i=0; i < NINDI_STD; i++)
        if (!strcmp(name.toLatin1().constData(), indi_std[i]))
            return true;

    return false;
}

ISD::GDInterface * INDIListener::getDevice(const QString &name)
{
    foreach(ISD::GDInterface *gi, devices)
        if (!strcmp(gi->getDeviceName(), name.toLatin1().constData()))
                return gi;

    return NULL;
}

void INDIListener::addClient(ClientManager *cm)
{
    //qDebug() << "add client for listener called " << endl;
    clients.append(cm);

    connect(cm, SIGNAL(newINDIDevice(DriverInfo*)), this, SLOT(processDevice(DriverInfo*)));
    connect(cm, SIGNAL(INDIDeviceRemoved(DriverInfo*)), this, SLOT(removeDevice(DriverInfo*)));

    connect(cm, SIGNAL(newINDIProperty(INDI::Property*)), this, SLOT(registerProperty(INDI::Property*)));
    connect(cm, SIGNAL(removeINDIProperty(INDI::Property*)), this, SLOT(removeProperty(INDI::Property*)), Qt::BlockingQueuedConnection);

    connect(cm, SIGNAL(newINDISwitch(ISwitchVectorProperty*)), this, SLOT(processSwitch(ISwitchVectorProperty*)));
    connect(cm, SIGNAL(newINDIText(ITextVectorProperty*)), this, SLOT(processText(ITextVectorProperty*)));
    connect(cm, SIGNAL(newINDINumber(INumberVectorProperty*)), this, SLOT(processNumber(INumberVectorProperty*)));
    connect(cm, SIGNAL(newINDILight(ILightVectorProperty*)), this, SLOT(processLight(ILightVectorProperty*)));
    connect(cm, SIGNAL(newINDIBLOB(IBLOB*)), this, SLOT(processBLOB(IBLOB*)));

}

void INDIListener::removeClient(ClientManager *cm)
{
    QList<ISD::GDInterface *>::iterator it = devices.begin();
    clients.removeOne(cm);

    while (it != devices.end())
    {
        if ( (*it)->getDriverInfo()->getClientManager() == cm)
        {
            cm->disconnect(this);
            it = devices.erase(it);
        }
      else
            ++it;
    }
}

void INDIListener::processDevice(DriverInfo *dv)
{
    //qDebug() << "process Device called for " << dv->getBaseDevice()->getDeviceName() << endl;

    ISD::GDInterface *gd = new ISD::GenericDevice(dv);

    //qDebug() << "Unique label for dv " << dv->getName() << " is: " << dv->getUniqueLabel() << endl;

    devices.append(gd);

    emit newDevice(gd);
}

void INDIListener::removeDevice(DriverInfo *dv)
{
    foreach(ISD::GDInterface *gd, devices)
    {
        if (dv->getUniqueLabel() == gd->getDeviceName() || dv->getDriverSource() == HOST_SOURCE)
        {
            emit deviceRemoved(gd);
            devices.removeOne(gd);
            delete(gd);

            if (dv->getDriverSource() != HOST_SOURCE)
                return;
        }
    }
}

void INDIListener::registerProperty(INDI::Property *prop)
{

    foreach(ISD::GDInterface *gd, devices)
    {
        if (!strcmp(gd->getDeviceName(), prop->getDeviceName() ))
        {
            if ( gd->getType() != KSTARS_TELESCOPE && (!strcmp(prop->getName(), "EQUATORIAL_EOD_COORD") || !strcmp(prop->getName(), "HORIZONTAL_COORD")) )
            {
                devices.removeOne(gd);
                gd = new ISD::Telescope(gd);
                devices.append(gd);
                emit newTelescope(gd);
             }
            else if (gd->getType() != KSTARS_CCD && (!strcmp(prop->getName(), "CCD_EXPOSURE_REQUEST")))
            {
                devices.removeOne(gd);
                gd = new ISD::CCD(gd);
                devices.append(gd);
                emit newCCD(gd);
            }
            else if (gd->getType() != KSTARS_FILTER && !strcmp(prop->getName(), "FILTER_SLOT"))
            {
                devices.removeOne(gd);
                gd = new ISD::Filter(gd);
                devices.append(gd);
                emit newFilter(gd);
            }
            else if (gd->getType() != KSTARS_FOCUSER && !strcmp(prop->getName(), "FOCUS_MOTION"))
            {
                devices.removeOne(gd);
                gd = new ISD::Focuser(gd);
                devices.append(gd);
                emit newFocuser(gd);
            }

            gd->registerProperty(prop);
            break;
        }
    }
}

void INDIListener::removeProperty(INDI::Property *prop)
{

    if (prop == NULL)
        return;

    foreach(ISD::GDInterface *gd, devices)
    {
        if (!strcmp(gd->getDeviceName(), prop->getDeviceName() ))
        {
            gd->removeProperty(prop);
            return;
        }
    }
}

void INDIListener::processSwitch(ISwitchVectorProperty * svp)
{

    foreach(ISD::GDInterface *gd, devices)
    {
        if (!strcmp(gd->getDeviceName(), svp->device))
        {
            gd->processSwitch(svp);
            break;
        }
    }

}

void INDIListener::processNumber(INumberVectorProperty * nvp)
{
    foreach(ISD::GDInterface *gd, devices)
    {
        if (!strcmp(gd->getDeviceName(), nvp->device))
        {
            gd->processNumber(nvp);
            break;
        }
    }
}

void INDIListener::processText(ITextVectorProperty * tvp)
{
    foreach(ISD::GDInterface *gd, devices)
    {
        if (!strcmp(gd->getDeviceName(), tvp->device))
        {
            gd->processText(tvp);
            break;
        }
    }
}

void INDIListener::processLight(ILightVectorProperty * lvp)
{

    foreach(ISD::GDInterface *gd, devices)
    {
        if (!strcmp(gd->getDeviceName(), lvp->device))
        {
            gd->processLight(lvp);
            break;
        }
    }
}

void INDIListener::processBLOB(IBLOB* bp)
{
    foreach(ISD::GDInterface *gd, devices)
    {
        if (!strcmp(gd->getDeviceName(), bp->bvp->device))
        {
            gd->processBLOB(bp);
            break;
        }
    }
}

#include "indilistener.moc"
