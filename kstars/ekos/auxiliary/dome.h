/*  Ekos Dome interface
    Copyright (C) 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#pragma once

#include "indi/indistd.h"
#include "indi/indidome.h"

#include <QtDBus>

namespace Ekos
{
/**
 * @class Dome
 * @short Supports basic dome functions
 *
 * @author Jasem Mutlaq
 * @version 1.0
 */
class Dome : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kstars.Ekos.Dome")
    Q_PROPERTY(ISD::Dome::Status status READ status NOTIFY newStatus)
    Q_PROPERTY(ISD::ParkStatus parkStatus READ parkStatus NOTIFY newParkStatus)
    Q_PROPERTY(bool canPark READ canPark)
    Q_PROPERTY(bool canAbsoluteMove READ canAbsoluteMove)
    Q_PROPERTY(double azimuthPosition READ azimuthPosition WRITE setAzimuthPosition NOTIFY azimuthPositionChanged)

  public:
    Dome();
    virtual ~Dome() override = default;

    /**
     * @defgroup DomeDBusInterface Ekos DBus Interface - Dome Interface
     * Ekos::Dome interface provides advanced basic dome operations.
     */

    /*@{*/

    /**
     * DBUS interface function.
     * Abort dome
     */
    Q_SCRIPTABLE bool abort();

    /**
     * DBUS interface function.
     * Can dome park?
     */
    Q_SCRIPTABLE bool canPark();

    /**
     * DBUS interface function.
     * Can dome move to an absolute azimuth position?
     */
    Q_SCRIPTABLE bool canAbsoluteMove();

    /**
     * DBUS interface function.
     * Park dome
     */
    Q_SCRIPTABLE bool park();

    /**
     * DBUS interface function.
     * Park dome
     */
    Q_SCRIPTABLE bool unpark();

    /**
     * DBUS interface function.
     * Get the dome park status
     */
    //Q_SCRIPTABLE ParkingStatus getParkingStatus();

    /**
     * DBUS interface function.
     * Check if the dome is in motion
     */
    Q_SCRIPTABLE bool isMoving();


    Q_SCRIPTABLE double azimuthPosition();
    Q_SCRIPTABLE void setAzimuthPosition(double position);

    /** @}*/

    /**
     * @brief setDome set the dome device
     * @param newDome pointer to Dome device.
     */
    void setDome(ISD::GDInterface *newDome);

    /**
     * @brief setTelescope Set the telescope device. This is only used to sync ACTIVE_TELESCOPE to the current active telescope.
     * @param newTelescope pointer to telescope device.
     */
    void setTelescope(ISD::GDInterface *newTelescope);

    ISD::Dome::Status status() { return m_Status; }
    ISD::ParkStatus parkStatus() { return m_ParkStatus; }

  signals:
    void newStatus(ISD::Dome::Status status);
    void newParkStatus(ISD::ParkStatus status);
    void azimuthPositionChanged(double position);
    void ready();

  private:
    // Devices needed for Dome operation
    ISD::Dome *currentDome { nullptr };

    ISD::Dome::Status m_Status { ISD::Dome::DOME_IDLE };
    ISD::ParkStatus m_ParkStatus { ISD::PARK_UNKNOWN };
};
}
