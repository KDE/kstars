/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later

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
static const char *indi_std[NINDI_STD] = { "CONNECTION",
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
                                           "FLAT_LIGHT_INTENSITY"
                                         };

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
    for (auto &oneDevice : devices)
    {
        if (oneDevice->getDeviceName() == name)
            return oneDevice;
    }
    return nullptr;
}

void INDIListener::addClient(ClientManager *cm)
{
    qCDebug(KSTARS_INDI) << "INDIListener: Adding a new client manager to INDI listener..";

    clients.append(cm);

    connect(cm, &ClientManager::newINDIDevice, this, &INDIListener::processDevice, Qt::BlockingQueuedConnection);
    //connect(cm, &ClientManager::newINDIDevice, this, &INDIListener::processDevice);
    connect(cm, &ClientManager::newINDIProperty, this, &INDIListener::registerProperty);

    connect(cm, &ClientManager::removeINDIDevice, this, &INDIListener::removeDevice);
    //connect(cm, &ClientManager::removeINDIProperty, this, &INDIListener::removeProperty, Qt::BlockingQueuedConnection);
    connect(cm, &ClientManager::removeINDIProperty, this, &INDIListener::removeProperty);

    connect(cm, &ClientManager::newINDISwitch, this, &INDIListener::processSwitch);
    connect(cm, &ClientManager::newINDIText, this, &INDIListener::processText);
    connect(cm, &ClientManager::newINDINumber, this, &INDIListener::processNumber);
    connect(cm, &ClientManager::newINDILight, this, &INDIListener::processLight);
    connect(cm, &ClientManager::newINDIBLOB, this, &INDIListener::processBLOB);
    connect(cm, &ClientManager::newINDIUniversalMessage, this, &INDIListener::processUniversalMessage);
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
            //            // If we have multiple devices per driver, we need to remove them all
            //            if (dv->getAuxInfo().value("mdpd", false).toBool() == true)
            //            {
            //                while (it != devices.end())
            //                {
            //                    if (dv->getDevice((*it)->getDeviceName()) != nullptr)
            //                    {
            //                        it = devices.erase(it);
            //                    }
            //                    else
            //                        break;
            //                }
            //            }
            //            else
            //                it = devices.erase(it);

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
    ClientManager *cm = qobject_cast<ClientManager *>(sender());
    Q_ASSERT_X(cm, __FUNCTION__, "Client manager is not valid.");

    qCDebug(KSTARS_INDI) << "INDIListener: New device" << dv->getDeviceName();

    ISD::GDInterface *gd = new ISD::GenericDevice(*dv, cm);

    devices.append(gd);

    emit newDevice(gd);
}

//void INDIListener::removeDevice(DeviceInfo *dv)
//{
//    qCDebug(KSTARS_INDI) << "INDIListener: Removing device" << dv->getBaseDevice()->getDeviceName() << "with unique label "
//                         << dv->getDriverInfo()->getUniqueLabel();

//    foreach (ISD::GDInterface *gd, devices)
//    {
//        if (gd->getDeviceInfo() == dv)
//        {
//            emit deviceRemoved(gd);
//            devices.removeOne(gd);
//            delete (gd);
//        }
//    }
//}

void INDIListener::removeDevice(const QString &deviceName)
{
    qCDebug(KSTARS_INDI) << "INDIListener: Removing device" << deviceName;

    for (ISD::GDInterface *oneDevice : devices)
    {
        if (oneDevice->getDeviceName() == deviceName)
        {
            emit deviceRemoved(oneDevice);
            devices.removeOne(oneDevice);
            oneDevice->deleteLater();
            return;
        }
    }
}

void INDIListener::registerProperty(INDI::Property prop)
{
    if (!prop.getRegistered())
        return;

    qCDebug(KSTARS_INDI) << "<" << prop.getDeviceName() << ">: <" << prop.getName() << ">";

    for (auto oneDevice : devices)
    {
        if (oneDevice->getDeviceName() == prop.getDeviceName())
        {
            if (prop.isNameMatch("ON_COORD_SET") ||
                    prop.isNameMatch("EQUATORIAL_EOD_COORD") ||
                    prop.isNameMatch("EQUATORIAL_COORD") ||
                    prop.isNameMatch("HORIZONTAL_COORD"))
            {
                if (oneDevice->getType() == KSTARS_UNKNOWN)
                {
                    devices.removeOne(oneDevice);
                    oneDevice = new ISD::Telescope(oneDevice);
                    devices.append(oneDevice);
                }

                emit newTelescope(oneDevice);
            }
            else if (prop.isNameMatch("CCD_EXPOSURE"))
            {
                // Only register a CCD device if the interface explicitly contains CCD_INTERFACE
                // and only if the device type is not already known.
                // If the device type is alredy KSTARS_CCD then no need to remove and re-parent
                // this happens in the case of disconnect/reconnect
                if (oneDevice->getType() == KSTARS_UNKNOWN &&
                        (oneDevice->getDriverInterface() & INDI::BaseDevice::CCD_INTERFACE))
                {
                    devices.removeOne(oneDevice);
                    oneDevice = new ISD::CCD(oneDevice);
                    devices.append(oneDevice);
                }

                emit newCCD(oneDevice);
            }
            else if (prop.isNameMatch("FILTER_NAME"))
            {
                if (oneDevice->getType() == KSTARS_UNKNOWN &&
                        !(oneDevice->getDriverInterface() & INDI::BaseDevice::CCD_INTERFACE))
                {
                    devices.removeOne(oneDevice);
                    oneDevice = new ISD::Filter(oneDevice);
                    devices.append(oneDevice);
                }

                emit newFilter(oneDevice);
            }
            else if (prop.isNameMatch("FOCUS_MOTION"))
            {
                if (oneDevice->getType() == KSTARS_UNKNOWN &&
                        !(oneDevice->getDriverInterface() & INDI::BaseDevice::CCD_INTERFACE))
                {
                    devices.removeOne(oneDevice);
                    oneDevice = new ISD::Focuser(oneDevice);
                    devices.append(oneDevice);
                }

                emit newFocuser(oneDevice);
            }

            else if (prop.isNameMatch("DOME_SHUTTER") ||
                     prop.isNameMatch("DOME_MOTION"))
            {
                if (oneDevice->getType() == KSTARS_UNKNOWN)
                {
                    devices.removeOne(oneDevice);
                    oneDevice = new ISD::Dome(oneDevice);
                    devices.append(oneDevice);
                }

                emit newDome(oneDevice);
            }
            else if (prop.isNameMatch("WEATHER_STATUS"))
            {
                if (oneDevice->getType() == KSTARS_UNKNOWN)
                {
                    devices.removeOne(oneDevice);
                    oneDevice = new ISD::Weather(oneDevice);
                    devices.append(oneDevice);
                }

                emit newWeather(oneDevice);
            }
            else if (prop.isNameMatch("CAP_PARK"))
            {
                if (oneDevice->getType() == KSTARS_UNKNOWN)
                {
                    devices.removeOne(oneDevice);
                    oneDevice = new ISD::DustCap(oneDevice);
                    devices.append(oneDevice);
                }

                emit newDustCap(oneDevice);
            }
            else if (prop.isNameMatch("FLAT_LIGHT_CONTROL"))
            {
                // If light box part of dust cap
                if (oneDevice->getType() == KSTARS_UNKNOWN)
                {
                    if (oneDevice->getBaseDevice()->getDriverInterface() & INDI::BaseDevice::DUSTCAP_INTERFACE)
                    {
                        devices.removeOne(oneDevice);
                        oneDevice = new ISD::DustCap(oneDevice);
                        devices.append(oneDevice);

                        emit newDustCap(oneDevice);
                    }
                    // If stand-alone light box
                    else
                    {
                        devices.removeOne(oneDevice);
                        oneDevice = new ISD::LightBox(oneDevice);
                        devices.append(oneDevice);

                        emit newLightBox(oneDevice);
                    }
                }
            }

            if (prop.isNameMatch("TELESCOPE_TIMED_GUIDE_WE"))
            {
                ISD::ST4 *st4Driver = new ISD::ST4(oneDevice->getBaseDevice(), oneDevice->getDriverInfo()->getClientManager());
                st4Devices.append(st4Driver);
                emit newST4(st4Driver);
            }

            oneDevice->registerProperty(prop);
            break;
        }
    }
}

void INDIListener::removeProperty(const QString &device, const QString &name)
{
    for (auto &oneDevice : devices)
    {
        if (oneDevice->getDeviceName() == device)
        {
            oneDevice->removeProperty(name);
            return;
        }
    }
}

void INDIListener::processSwitch(ISwitchVectorProperty *svp)
{
    for (auto &oneDevice : devices)
    {
        if (oneDevice->getDeviceName() == svp->device)
        {
            oneDevice->processSwitch(svp);
            break;
        }
    }
}

void INDIListener::processNumber(INumberVectorProperty *nvp)
{
    for (auto &oneDevice : devices)
    {
        if (oneDevice->getDeviceName() == nvp->device)
        {
            oneDevice->processNumber(nvp);
            break;
        }
    }
}

void INDIListener::processText(ITextVectorProperty *tvp)
{
    for (auto &oneDevice : devices)
    {
        if (oneDevice->getDeviceName() == tvp->device)
        {
            oneDevice->processText(tvp);
            break;
        }
    }
}

void INDIListener::processLight(ILightVectorProperty *lvp)
{
    for (auto &oneDevice : devices)
    {
        if (oneDevice->getDeviceName() == lvp->device)
        {
            oneDevice->processLight(lvp);
            break;
        }
    }
}

void INDIListener::processBLOB(IBLOB *bp)
{
    for (auto &oneDevice : devices)
    {
        if (oneDevice->getDeviceName() == bp->bvp->device)
        {
            oneDevice->processBLOB(bp);
            break;
        }
    }
}

void INDIListener::processMessage(INDI::BaseDevice *dp, int messageID)
{
    for (auto &oneDevice : devices)
    {
        if (oneDevice->getDeviceName() == dp->getDeviceName())
        {
            oneDevice->processMessage(messageID);
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
        displayMessage = displayMessage.mid(colonIndex + 2);

    // Special case for Alignment since it is not tied to a device
    if (displayMessage.startsWith("[ALIGNMENT]"))
    {
        qCDebug(KSTARS_INDI) << "AlignmentSubSystem:" << displayMessage;
        return;
    }

    if (Options::messageNotificationINDI())
    {
        KSNotification::event(QLatin1String("IndiServerMessage"), displayMessage, KSNotification::EVENT_WARN);
    }
    else
    {
        KSNotification::transient(displayMessage, i18n("INDI Server Message"));
    }
}
