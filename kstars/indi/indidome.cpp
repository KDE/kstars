/*
    SPDX-FileCopyrightText: 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <basedevice.h>
#include <KActionCollection>
#include <KNotification>
#include <QAction>
#include <qdbusmetatype.h>

#include "indidome.h"
#include "kstars.h"
#include "domeadaptor.h"

namespace ISD
{

const QList<const char *> Dome::domeStates = { I18N_NOOP("Idle"), I18N_NOOP("Moving clockwise"), I18N_NOOP("Moving counter clockwise"),
                                               I18N_NOOP("Tracking"), I18N_NOOP("Parking"), I18N_NOOP("UnParking"), I18N_NOOP("Parked"),
                                               I18N_NOOP("Error")
                                             };

Dome::Dome(GenericDevice *parent) : ConcreteDevice(parent)
{
    qRegisterMetaType<ISD::Dome::Status>("ISD::Dome::Status");
    qDBusRegisterMetaType<ISD::Dome::Status>();

    qRegisterMetaType<ISD::Dome::ShutterStatus>("ISD::Dome::ShutterStatus");
    qDBusRegisterMetaType<ISD::Dome::ShutterStatus>();

    new DomeAdaptor(this);
    m_DBusObjectPath = QString("/KStars/INDI/Dome/%1").arg(getID());
    QDBusConnection::sessionBus().registerObject(m_DBusObjectPath, this);
}

void Dome::registerProperty(INDI::Property prop)
{
    if (!prop.getRegistered())
        return;

    if (prop.isNameMatch("ABS_DOME_POSITION"))
    {
        m_CanAbsMove = true;
    }
    else if (prop.isNameMatch("REL_DOME_POSITION"))
    {
        m_CanRelMove = true;
    }
    else if (prop.isNameMatch("DOME_ABORT_MOTION"))
    {
        m_CanAbort = true;
    }
    else if (prop.isNameMatch("DOME_SHUTTER"))
    {
        m_HasShutter = true;
    }

    ConcreteDevice::registerProperty(prop);
}

void Dome::processNumber(INDI::Property prop)
{
    auto nvp = prop.getNumber();
    if (nvp->isNameMatch("ABS_DOME_POSITION"))
    {
        emit positionChanged(nvp->at(0)->getValue());
    }
}

void Dome::processSwitch(INDI::Property prop)
{
    auto svp = prop.getSwitch();
    if (svp->isNameMatch("CONNECTION"))
    {
        auto conSP = svp->findWidgetByName("CONNECT");
        if (conSP)
        {
            if (conSP->getState() == ISS_ON)
                KStars::Instance()->slotSetDomeEnabled(true);
            else
            {
                KStars::Instance()->slotSetDomeEnabled(false);

                m_CanAbsMove = false;
                m_CanPark = false;
            }
        }
    }
    else if (svp->isNameMatch("DOME_PARK"))
    {
        m_CanPark = true;

        auto sp = svp->findWidgetByName("PARK");
        if (sp)
        {
            if (svp->getState() == IPS_ALERT)
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
            else if (svp->getState() == IPS_BUSY && sp->getState() == ISS_ON && m_ParkStatus != PARK_PARKING)
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
            else if (svp->getState() == IPS_BUSY && sp->getState() == ISS_OFF && m_ParkStatus != PARK_UNPARKING)
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
            else if (svp->getState() == IPS_OK && sp->getState() == ISS_ON && m_ParkStatus != PARK_PARKED)
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
            else if ( (svp->getState() == IPS_OK || svp->getState() == IPS_IDLE) && sp->s == ISS_OFF && m_ParkStatus != PARK_UNPARKED)
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
    else if (svp->isNameMatch("DOME_MOTION"))
    {
        Status lastStatus = m_Status;

        if (svp->getState() == IPS_BUSY && lastStatus != DOME_MOVING_CW && lastStatus != DOME_MOVING_CCW
                && lastStatus != DOME_PARKING
                && lastStatus != DOME_UNPARKING)
        {
            m_Status = svp->at(0)->getState() == ISS_ON ? DOME_MOVING_CW : DOME_MOVING_CCW;
            emit newStatus(m_Status);

            // rolloff roofs: cw = opening = unparking, ccw = closing = parking
            if (!canAbsoluteMove() && !canRelativeMove())
            {
                m_ParkStatus = (m_Status == DOME_MOVING_CW) ? PARK_UNPARKING : PARK_PARKING;
                emit newParkStatus(m_ParkStatus);
            }
        }
        else if (svp->getState() == IPS_OK && (lastStatus == DOME_MOVING_CW || lastStatus == DOME_MOVING_CCW))
        {
            m_Status = DOME_TRACKING;
            emit newStatus(m_Status);
        }
        else if (svp->getState() == IPS_IDLE && lastStatus != DOME_IDLE)
        {
            m_Status = DOME_IDLE;
            emit newStatus(m_Status);
        }
        else if (svp->getState() == IPS_ALERT)
        {
            m_Status = DOME_ERROR;
            emit newStatus(m_Status);
        }
    }
    else if (svp->isNameMatch("DOME_SHUTTER"))
    {
        if (svp->getState() == IPS_ALERT)
        {
            emit newShutterStatus(SHUTTER_ERROR);

            // If alert, set shutter status to whatever it was opposite to. That is, if it was opening and failed
            // then we set status to closed since it did not successfully complete opening.
            if (m_ShutterStatus == SHUTTER_CLOSING)
                m_ShutterStatus = SHUTTER_OPEN;
            else
                m_ShutterStatus = SHUTTER_CLOSED;

            emit newShutterStatus(m_ShutterStatus);
        }

        ShutterStatus status = parseShutterStatus(prop);

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
    else if (svp->isNameMatch("DOME_AUTOSYNC"))
    {
        auto sp = svp->findWidgetByName("DOME_AUTOSYNC_ENABLE");
        if (sp != nullptr)
            emit newAutoSyncStatus(sp->s == ISS_ON);
    }
}

bool Dome::abort()
{
    if (m_CanAbort == false)
        return false;

    auto motionSP = getSwitch("DOME_ABORT_MOTION");

    if (!motionSP)
        return false;

    auto abortSW = motionSP->findWidgetByName("ABORT");

    if (!abortSW)
        return false;

    abortSW->setState(ISS_ON);
    sendNewProperty(motionSP);

    return true;
}

bool Dome::park()
{
    auto parkSP = getSwitch("DOME_PARK");

    if (!parkSP)
        return false;

    auto parkSW = parkSP->findWidgetByName("PARK");

    if (!parkSW)
        return false;

    parkSP->reset();
    parkSW->setState(ISS_ON);
    sendNewProperty(parkSP);

    return true;
}

bool Dome::unpark()
{
    auto parkSP = getSwitch("DOME_PARK");

    if (!parkSP)
        return false;

    auto parkSW = parkSP->findWidgetByName("UNPARK");

    if (!parkSW)
        return false;

    parkSP->reset();
    parkSW->setState(ISS_ON);
    sendNewProperty(parkSP);

    return true;
}

bool Dome::isMoving() const
{
    auto motionSP = getSwitch("DOME_MOTION");

    if (motionSP && motionSP->getState() == IPS_BUSY)
        return true;

    return false;
}

double Dome::position() const
{
    auto az = getNumber("ABS_DOME_POSITION");

    if (!az)
        return -1;
    else
        return az->at(0)->getValue();
}

bool Dome::setPosition(double position)
{
    auto az = getNumber("ABS_DOME_POSITION");

    if (!az)
        return false;

    az->at(0)->setValue(position);
    sendNewProperty(az);
    return true;
}

bool Dome::setRelativePosition(double position)
{
    auto azDiff = getNumber("REL_DOME_POSITION");
    if (!azDiff)
        return false;

    azDiff->at(0)->setValue(position);
    sendNewProperty(azDiff);
    return true;
}

bool Dome::isAutoSync()
{
    auto autosync = getSwitch("DOME_AUTOSYNC");
    if (!autosync)
        return false;

    auto autosyncSW = autosync->findWidgetByName("DOME_AUTOSYNC_ENABLE");
    if (!autosyncSW)
        return false;
    else
        return (autosyncSW->s == ISS_ON);
}

bool Dome::setAutoSync(bool activate)
{
    auto autosync = getSwitch("DOME_AUTOSYNC");
    if (!autosync)
        return false;

    auto autosyncSW = autosync->findWidgetByName(activate ? "DOME_AUTOSYNC_ENABLE" : "DOME_AUTOSYNC_DISABLE");
    if (!autosyncSW)
        return false;

    autosync->reset();
    autosyncSW->setState(ISS_ON);
    sendNewProperty(autosync);

    return true;
}

bool Dome::moveDome(DomeDirection dir, DomeMotionCommand operation)
{
    auto domeMotion = getSwitch("DOME_MOTION");
    if (!domeMotion)
        return false;

    auto opSwitch = domeMotion->findWidgetByName(dir == DomeDirection::DOME_CW ? "DOME_CW" : "DOME_CCW");
    if (!opSwitch)
        return false;

    domeMotion->reset();
    opSwitch->setState(operation == DomeMotionCommand::MOTION_START ? ISS_ON : ISS_OFF);
    sendNewProperty(domeMotion);
    return true;
}

bool Dome::controlShutter(bool open)
{
    auto shutterSP = getSwitch("DOME_SHUTTER");
    if (!shutterSP)
        return false;

    auto shutterSW = shutterSP->findWidgetByName(open ? "SHUTTER_OPEN" : "SHUTTER_CLOSE");
    if (!shutterSW)
        return false;

    shutterSP->reset();
    shutterSW->setState(ISS_ON);
    sendNewProperty(shutterSP);

    return true;
}

Dome::ShutterStatus Dome::shutterStatus()
{
    auto shutterSP = getSwitch("DOME_SHUTTER");

    return parseShutterStatus(shutterSP);
}

Dome::ShutterStatus Dome::parseShutterStatus(INDI::Property prop)
{
    if (prop.isValid() == false)
        return SHUTTER_UNKNOWN;

    auto svp = prop.getSwitch();

    auto sp = svp->findWidgetByName("SHUTTER_OPEN");
    if (sp == nullptr)
        return SHUTTER_UNKNOWN;

    if (svp->getState() == IPS_ALERT)
        return SHUTTER_ERROR;
    else if (svp->getState() == IPS_BUSY)
        return (sp->s == ISS_ON) ? SHUTTER_OPENING : SHUTTER_CLOSING;
    else if (svp->getState() == IPS_OK)
        return (sp->s == ISS_ON) ? SHUTTER_OPEN : SHUTTER_CLOSED;

    // this should not happen
    return SHUTTER_UNKNOWN;
}

const QString Dome::getStatusString(Dome::Status status, bool translated)
{
    return translated ? i18n(domeStates[status]) : domeStates[status];
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

QDBusArgument &operator<<(QDBusArgument &argument, const ISD::Dome::ShutterStatus &source)
{
    argument.beginStructure();
    argument << static_cast<int>(source);
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, ISD::Dome::ShutterStatus &dest)
{
    int a;
    argument.beginStructure();
    argument >> a;
    argument.endStructure();
    dest = static_cast<ISD::Dome::ShutterStatus>(a);
    return argument;
}

