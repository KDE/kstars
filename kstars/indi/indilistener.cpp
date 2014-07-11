/*  INDI Listener
    Copyright (C) 2012 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    Handle INDI Standard properties.
 */

#include <QDebug>

#include <KMessageBox>
#include <QStatusBar>

#include <baseclient.h>
#include <basedevice.h>

#include "indilistener.h"
#include "indicommon.h"
#include "inditelescope.h"
#include "indifocuser.h"
#include "indiccd.h"
#include "indifilter.h"
#include "clientmanager.h"
#include "driverinfo.h"
#include "deviceinfo.h"
#include "fitsviewer/fitsviewer.h"

#include "kstars.h"
#include "Options.h"

#define NINDI_STD	28

/* INDI standard property used across all clients to enable interoperability. */
const char * indi_std[NINDI_STD] =
    {"CONNECTION", "DEVICE_PORT", "TIME_UTC", "TIME_LST", "GEOGRAPHIC_COORD", "EQUATORIAL_COORD",
     "EQUATORIAL_EOD_COORD", "EQUATORIAL_EOD_COORD_REQUEST", "HORIZONTAL_COORD", "TELESCOPE_ABORT_MOTION", "ON_COORD_SET",
     "SOLAR_SYSTEM", "TELESCOPE_MOTION_NS", "TELESCOPE_MOTION_WE",  "TELESCOPE_PARK", "CCD_EXPOSURE",
     "CCD_TEMPERATURE", "CCD_FRAME", "CCD_FRAME_TYPE", "CCD_BINNING", "CCD_INFO", "VIDEO_STREAM",
     "RAW_STREAM", "IMAGE_STREAM", "FOCUS_SPEED", "FOCUS_MOTION", "FOCUS_TIMER", "FILTER_SLOT" };

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

    connect(cm, SIGNAL(newINDIDevice(DeviceInfo*)), this, SLOT(processDevice(DeviceInfo*)));

    connect(cm, SIGNAL(INDIDeviceRemoved(DeviceInfo*)), this, SLOT(removeDevice(DeviceInfo*)));

    connect(cm, SIGNAL(newINDIProperty(INDI::Property*)), this, SLOT(registerProperty(INDI::Property*)));
    //connect(cm, SIGNAL(removeINDIProperty(INDI::Property*)), this, SLOT(removeProperty(INDI::Property*)), Qt::BlockingQueuedConnection);
    connect(cm, SIGNAL(removeINDIProperty(INDI::Property*)), this, SLOT(removeProperty(INDI::Property*)));

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
        DriverInfo *dv = (*it)->getDriverInfo();
        //if ( (*it)->getDriverInfo()->getClientManager() == cm)
        if (dv && cm->isDriverManaged(dv))
        {
            cm->removeManagedDriver(dv);
            cm->disconnect(this);
            it = devices.erase(it);
        }
      else
            ++it;
    }
}

void INDIListener::processDevice(DeviceInfo *dv)
{
    //qDebug() << "process Device called for " << dv->getBaseDevice()->getDeviceName() << endl;

    ISD::GDInterface *gd = new ISD::GenericDevice(dv);

    //qDebug() << "Unique label for dv " << dv->getName() << " is: " << dv->getUniqueLabel() << endl;

    devices.append(gd);

    emit newDevice(gd);
}

void INDIListener::removeDevice(DeviceInfo *dv)
{
    foreach(ISD::GDInterface *gd, devices)
    {
        if (dv->getDriverInfo()->getUniqueLabel() == gd->getDeviceName() || dv->getDriverInfo()->getDriverSource() == HOST_SOURCE)
        {           
            emit deviceRemoved(gd);
            devices.removeOne(gd);
            delete(gd);

            if (dv->getDriverInfo()->getDriverSource() != HOST_SOURCE)
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
            if ( gd->getType() == KSTARS_UNKNOWN && (!strcmp(prop->getName(), "EQUATORIAL_EOD_COORD") || !strcmp(prop->getName(), "HORIZONTAL_COORD")) )
            {
                devices.removeOne(gd);
                gd = new ISD::Telescope(gd);
                devices.append(gd);
                emit newTelescope(gd);
             }
            else if (gd->getType() == KSTARS_UNKNOWN && (!strcmp(prop->getName(), "CCD_EXPOSURE")))
            {
                devices.removeOne(gd);
                gd = new ISD::CCD(gd);
                devices.append(gd);
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
