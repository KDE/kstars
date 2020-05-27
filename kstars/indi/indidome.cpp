/*  INDI Dome
    Copyright (C) 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include <basedevice.h>
#include <KActionCollection>
#include <KNotification>
#include <QAction>
#include <QtDBus/qdbusmetatype.h>

#include "indidome.h"
#include "kstars.h"
#include "clientmanager.h"

namespace ISD
{

Dome::Dome(GDInterface *iPtr) : DeviceDecorator(iPtr)
{
    dType = KSTARS_DOME;
    qRegisterMetaType<ISD::Dome::Status>("ISD::Dome::Status");
    qDBusRegisterMetaType<ISD::Dome::Status>();

    readyTimer.reset(new QTimer());
    readyTimer.get()->setInterval(250);
    readyTimer.get()->setSingleShot(true);
    connect(readyTimer.get(), &QTimer::timeout, this, &Dome::ready);
}

void Dome::registerProperty(INDI::Property *prop)
{
    if (!prop->getRegistered())
        return;

    if (isConnected())
        readyTimer.get()->start();

    if (!strcmp(prop->getName(), "DOME_PARK"))
    {
        ISwitchVectorProperty *svp = prop->getSwitch();

        m_CanPark = true;

        if (svp)
        {
            ISwitch *sp = IUFindSwitch(svp, "PARK");
            if (sp)
            {
                if ((sp->s == ISS_ON) && svp->s == IPS_OK)
                {
                    m_ParkStatus = PARK_PARKED;
                    m_Status = DOME_PARKED;
                    emit newParkStatus(m_ParkStatus);

                    QAction *parkAction = KStars::Instance()->actionCollection()->action("dome_park");
                    if (parkAction)
                        parkAction->setEnabled(false);
                    QAction *unParkAction = KStars::Instance()->actionCollection()->action("dome_unpark");
                    if (unParkAction)
                        unParkAction->setEnabled(true);
                }
                else if ((sp->s == ISS_OFF) && svp->s == IPS_OK)
                {
                    m_ParkStatus = PARK_UNPARKED;
                    m_Status = DOME_IDLE;
                    emit newParkStatus(m_ParkStatus);

                    QAction *parkAction = KStars::Instance()->actionCollection()->action("dome_park");
                    if (parkAction)
                        parkAction->setEnabled(true);
                    QAction *unParkAction = KStars::Instance()->actionCollection()->action("dome_unpark");
                    if (unParkAction)
                        unParkAction->setEnabled(false);
                }
            }
        }
    }
    else if (!strcmp(prop->getName(), "ABS_DOME_POSITION"))
    {
        m_CanAbsMove = true;
    }
    else if (!strcmp(prop->getName(), "REL_DOME_POSITION"))
    {
        m_CanRelMove = true;
    }
    else if (!strcmp(prop->getName(), "DOME_ABORT_MOTION"))
    {
        m_CanAbort = true;
    }
    else if (!strcmp(prop->getName(), "DOME_SHUTTER"))
    {
        m_HasShutter = true;
    }

    DeviceDecorator::registerProperty(prop);
}

void Dome::processLight(ILightVectorProperty *lvp)
{
    DeviceDecorator::processLight(lvp);
}

void Dome::processNumber(INumberVectorProperty *nvp)
{
    if (!strcmp(nvp->name, "ABS_DOME_POSITION"))
    {
        emit azimuthPositionChanged(nvp->np[0].value);
    }

    DeviceDecorator::processNumber(nvp);
}

void Dome::processSwitch(ISwitchVectorProperty *svp)
{
    if (!strcmp(svp->name, "CONNECTION"))
    {
        ISwitch *conSP = IUFindSwitch(svp, "CONNECT");
        if (conSP)
        {
            if (isConnected() == false && conSP->s == ISS_ON)
                KStars::Instance()->slotSetDomeEnabled(true);
            else if (isConnected() && conSP->s == ISS_OFF)
            {
                KStars::Instance()->slotSetDomeEnabled(false);

                m_CanAbsMove = false;
                m_CanPark = false;
            }
        }
    }
    else if (!strcmp(svp->name, "DOME_PARK"))
    {
        m_CanPark = true;

        ISwitch *sp = IUFindSwitch(svp, "PARK");
        if (sp)
        {
            if (svp->s == IPS_ALERT)
            {
                m_ParkStatus = PARK_ERROR;
                emit newParkStatus(PARK_ERROR);

                // If alert, set park status to whatever it was opposite to. That is, if it was parking and failed
                // then we set status to unparked since it did not successfully complete parking.
                //                if (m_ParkStatus == PARK_PARKING)
                //                    m_ParkStatus = PARK_UNPARKED;
                //                else if (m_ParkStatus == PARK_UNPARKING)
                //                    m_ParkStatus = PARK_PARKED;

                //                emit newParkStatus(m_ParkStatus);

            }
            else if (svp->s == IPS_BUSY && sp->s == ISS_ON && m_ParkStatus != PARK_PARKING)
            {
                m_ParkStatus = PARK_PARKING;
                KNotification::event(QLatin1String("DomeParking"), i18n("Dome parking is in progress"));
                emit newParkStatus(m_ParkStatus);

                if (m_Status != DOME_PARKING)
                {
                    m_Status = DOME_PARKING;
                    emit newStatus(m_Status);
                }
            }
            else if (svp->s == IPS_BUSY && sp->s == ISS_OFF && m_ParkStatus != PARK_UNPARKING)
            {
                m_ParkStatus = PARK_UNPARKING;
                KNotification::event(QLatin1String("DomeUnparking"), i18n("Dome unparking is in progress"));
                emit newParkStatus(m_ParkStatus);

                if (m_Status != DOME_UNPARKING)
                {
                    m_Status = DOME_UNPARKING;
                    emit newStatus(m_Status);
                }
            }
            else if (svp->s == IPS_OK && sp->s == ISS_ON && m_ParkStatus != PARK_PARKED)
            {
                m_ParkStatus = PARK_PARKED;
                KNotification::event(QLatin1String("DomeParked"), i18n("Dome parked"));
                emit newParkStatus(m_ParkStatus);

                QAction *parkAction = KStars::Instance()->actionCollection()->action("dome_park");
                if (parkAction)
                    parkAction->setEnabled(false);
                QAction *unParkAction = KStars::Instance()->actionCollection()->action("dome_unpark");
                if (unParkAction)
                    unParkAction->setEnabled(true);

                if (m_Status != DOME_PARKED)
                {
                    m_Status = DOME_PARKED;
                    emit newStatus(m_Status);
                }

            }
            else if ( (svp->s == IPS_OK || svp->s == IPS_IDLE) && sp->s == ISS_OFF && m_ParkStatus != PARK_UNPARKED)
            {
                m_ParkStatus = PARK_UNPARKED;
                KNotification::event(QLatin1String("DomeUnparked"), i18n("Dome unparked"));

                QAction *parkAction = KStars::Instance()->actionCollection()->action("dome_park");
                if (parkAction)
                    parkAction->setEnabled(true);
                QAction *unParkAction = KStars::Instance()->actionCollection()->action("dome_unpark");
                if (unParkAction)
                    unParkAction->setEnabled(false);

                emit newParkStatus(m_ParkStatus);

                if (m_Status != DOME_IDLE)
                {
                    m_Status = DOME_IDLE;
                    emit newStatus(m_Status);
                }
            }
        }
    }
    else if (!strcmp(svp->name, "DOME_MOTION"))
    {
        Status lastStatus = m_Status;

        if (svp->s == IPS_BUSY && lastStatus != DOME_MOVING_CW && lastStatus != DOME_MOVING_CCW && lastStatus != DOME_PARKING
                && lastStatus != DOME_UNPARKING)
        {
            m_Status = svp->sp->s == ISS_ON ? DOME_MOVING_CW : DOME_MOVING_CCW;
            emit newStatus(m_Status);

            // rolloff roofs: cw = opening = unparking, ccw = closing = parking
            if (!canAbsMove() && !canRelMove())
            {
                m_ParkStatus = (m_Status == DOME_MOVING_CW) ? PARK_UNPARKING : PARK_PARKING;
                emit newParkStatus(m_ParkStatus);
            }
        }
        else if (svp->s == IPS_OK && (lastStatus == DOME_MOVING_CW || lastStatus == DOME_MOVING_CCW))
        {
            m_Status = DOME_TRACKING;
            emit newStatus(m_Status);
        }
        else if (svp->s == IPS_IDLE && lastStatus != DOME_IDLE)
        {
            m_Status = DOME_IDLE;
            emit newStatus(m_Status);
        }
        else if (svp->s == IPS_ALERT)
        {
            m_Status = DOME_ERROR;
            emit newStatus(m_Status);
        }
    }
    else if (!strcmp(svp->name, "DOME_SHUTTER"))
    {
        if (svp->s == IPS_ALERT)
        {
            emit newShutterStatus(SHUTTER_ERROR);

            // If alert, set shutter status to whatever it was opposite to. That is, if it was opening and failed
            // then we set status to closed since it did not successfully complete opening.
            if (m_ShutterStatus == SHUTTER_CLOSING)
                m_ShutterStatus = SHUTTER_OPEN;
            else if (m_ShutterStatus == SHUTTER_CLOSING)
                m_ShutterStatus = SHUTTER_CLOSED;

            emit newShutterStatus(m_ShutterStatus);
        }

        ShutterStatus status = shutterStatus(svp);

        switch (status)
        {
            case SHUTTER_CLOSING:
                if (m_ShutterStatus != SHUTTER_CLOSING)
                {
                    m_ShutterStatus = SHUTTER_CLOSING;
                    KNotification::event(QLatin1String("ShutterClosing"), i18n("Shutter closing is in progress"));
                    emit newShutterStatus(m_ShutterStatus);
                }
                break;
            case SHUTTER_OPENING:
                if (m_ShutterStatus != SHUTTER_OPENING)
                {
                    m_ShutterStatus = SHUTTER_OPENING;
                    KNotification::event(QLatin1String("ShutterOpening"), i18n("Shutter opening is in progress"));
                    emit newShutterStatus(m_ShutterStatus);
                }
                break;
            case SHUTTER_CLOSED:
                if (m_ShutterStatus != SHUTTER_CLOSED)
                {
                    m_ShutterStatus = SHUTTER_CLOSED;
                    KNotification::event(QLatin1String("ShutterClosed"), i18n("Shutter closed"));
                    emit newShutterStatus(m_ShutterStatus);
                }
                break;
            case SHUTTER_OPEN:
                if (m_ShutterStatus != SHUTTER_OPEN)
                {
                    m_ShutterStatus = SHUTTER_OPEN;
                    KNotification::event(QLatin1String("ShutterOpened"), i18n("Shutter opened"));
                    emit newShutterStatus(m_ShutterStatus);
                }
                break;
            default:
                break;
        }

        return;

    }
    else if (!strcmp(svp->name, "DOME_AUTOSYNC"))
    {
        ISwitch *sp = IUFindSwitch(svp, "DOME_AUTOSYNC_ENABLE");
        if (sp != nullptr)
            emit newAutoSyncStatus(sp->s == ISS_ON);
    }
    DeviceDecorator::processSwitch(svp);
}

void Dome::processText(ITextVectorProperty *tvp)
{
    DeviceDecorator::processText(tvp);
}

bool Dome::Abort()
{
    if (m_CanAbort == false)
        return false;

    ISwitchVectorProperty *motionSP = baseDevice->getSwitch("DOME_ABORT_MOTION");

    if (motionSP == nullptr)
        return false;

    ISwitch *abortSW = IUFindSwitch(motionSP, "ABORT");

    if (abortSW == nullptr)
        return false;

    abortSW->s = ISS_ON;
    clientManager->sendNewSwitch(motionSP);

    return true;
}

bool Dome::Park()
{
    ISwitchVectorProperty *parkSP = baseDevice->getSwitch("DOME_PARK");

    if (parkSP == nullptr)
        return false;

    ISwitch *parkSW = IUFindSwitch(parkSP, "PARK");

    if (parkSW == nullptr)
        return false;

    IUResetSwitch(parkSP);
    parkSW->s = ISS_ON;
    clientManager->sendNewSwitch(parkSP);

    return true;
}

bool Dome::UnPark()
{
    ISwitchVectorProperty *parkSP = baseDevice->getSwitch("DOME_PARK");

    if (parkSP == nullptr)
        return false;

    ISwitch *parkSW = IUFindSwitch(parkSP, "UNPARK");

    if (parkSW == nullptr)
        return false;

    IUResetSwitch(parkSP);
    parkSW->s = ISS_ON;
    clientManager->sendNewSwitch(parkSP);

    return true;
}

bool Dome::isMoving() const
{
    ISwitchVectorProperty *motionSP = baseDevice->getSwitch("DOME_MOTION");

    if (motionSP && motionSP->s == IPS_BUSY)
        return true;

    return false;
}

double Dome::azimuthPosition() const
{
    INumberVectorProperty *az = baseDevice->getNumber("ABS_DOME_POSITION");

    if (az == nullptr)
        return -1;
    else
        return az->np[0].value;
}

bool Dome::setAzimuthPosition(double position)
{
    INumberVectorProperty *az = baseDevice->getNumber("ABS_DOME_POSITION");

    if (az == nullptr)
        return false;

    az->np[0].value = position;
    clientManager->sendNewNumber(az);
    return true;
}

bool Dome::setRelativePosition(double position)
{
    INumberVectorProperty *azDiff = baseDevice->getNumber("REL_DOME_POSITION");

    if (azDiff == nullptr)
        return false;

    azDiff->np[0].value = position;
    clientManager->sendNewNumber(azDiff);
    return true;
}

bool Dome::isAutoSync()
{
    ISwitchVectorProperty *autosync = baseDevice->getSwitch("DOME_AUTOSYNC");

    if (autosync == nullptr)
        return false;

    ISwitch *autosyncSW = IUFindSwitch(autosync, "DOME_AUTOSYNC_ENABLE");

    if (autosync == nullptr)
        return false;
    else
        return (autosyncSW->s == ISS_ON);
}

bool Dome::setAutoSync(bool activate)
{
    ISwitchVectorProperty *autosync = baseDevice->getSwitch("DOME_AUTOSYNC");

    if (autosync == nullptr)
        return false;

    ISwitch *autosyncSW = IUFindSwitch(autosync, activate ? "DOME_AUTOSYNC_ENABLE" : "DOME_AUTOSYNC_DISABLE");
    if (autosyncSW == nullptr)
        return false;

    IUResetSwitch(autosync);
    autosyncSW->s = ISS_ON;
    clientManager->sendNewSwitch(autosync);

    return true;
}

bool Dome::moveDome(DomeDirection dir, DomeMotionCommand operation)
{
    ISwitchVectorProperty *domeMotion = baseDevice->getSwitch("DOME_MOTION");
    if (domeMotion == nullptr)
        return false;

    ISwitch *opSwitch = IUFindSwitch(domeMotion, dir == DomeDirection::DOME_CW ? "DOME_CW" : "DOME_CCW");
    IUResetSwitch(domeMotion);
    opSwitch->s = (operation == DomeMotionCommand::MOTION_START ? ISS_ON : ISS_OFF);

    clientManager->sendNewSwitch(domeMotion);

    return true;
}

bool Dome::ControlShutter(bool open)
{
    ISwitchVectorProperty *shutterSP = baseDevice->getSwitch("DOME_SHUTTER");

    if (shutterSP == nullptr)
        return false;

    ISwitch *shutterSW = IUFindSwitch(shutterSP, open ? "SHUTTER_OPEN" : "SHUTTER_CLOSE");

    if (shutterSW == nullptr)
        return false;

    IUResetSwitch(shutterSP);
    shutterSW->s = ISS_ON;
    clientManager->sendNewSwitch(shutterSP);

    return true;
}

Dome::ShutterStatus Dome::shutterStatus()
{
    ISwitchVectorProperty *shutterSP = baseDevice->getSwitch("DOME_SHUTTER");

    return shutterStatus(shutterSP);

}

Dome::ShutterStatus Dome::shutterStatus(ISwitchVectorProperty *svp)
{
    if (svp == nullptr)
        return SHUTTER_UNKNOWN;

    ISwitch *sp = IUFindSwitch(svp, "SHUTTER_OPEN");
    if (sp == nullptr)
        return SHUTTER_UNKNOWN;

    if (svp->s == IPS_ALERT)
        return SHUTTER_ERROR;
    else if (svp->s == IPS_BUSY)
        return (sp->s == ISS_ON) ? SHUTTER_OPENING : SHUTTER_CLOSING;
    else if (svp->s == IPS_OK)
        return (sp->s == ISS_ON) ? SHUTTER_OPEN : SHUTTER_CLOSED;

    // this should not happen
    return SHUTTER_UNKNOWN;
}

const QString Dome::getStatusString(Dome::Status status)
{
    switch (status)
    {
        case ISD::Dome::DOME_IDLE:
            return i18n("Idle");

        case ISD::Dome::DOME_PARKED:
            return i18n("Parked");

        case ISD::Dome::DOME_PARKING:
            return i18n("Parking");

        case ISD::Dome::DOME_UNPARKING:
            return i18n("UnParking");

        case ISD::Dome::DOME_MOVING_CW:
            return i18n("Moving clockwise");

        case ISD::Dome::DOME_MOVING_CCW:
            return i18n("Moving counter clockwise");

        case ISD::Dome::DOME_TRACKING:
            return i18n("Tracking");

        case ISD::Dome::DOME_ERROR:
            return i18n("Error");
    }

    return i18n("Error");
}


}

QDBusArgument &operator<<(QDBusArgument &argument, const ISD::Dome::Status &source)
{
    argument.beginStructure();
    argument << static_cast<int>(source);
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, ISD::Dome::Status &dest)
{
    int a;
    argument.beginStructure();
    argument >> a;
    argument.endStructure();
    dest = static_cast<ISD::Dome::Status>(a);
    return argument;
}
