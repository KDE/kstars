/*  Ekos DustCap interface
    Copyright (C) 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#pragma once

#include "indi/indistd.h"
#include "indi/indicap.h"

#include <QtDBus>

namespace Ekos
{
/**
 * @class DustCap
 * @short Supports basic DustCap functions (open/close) and optionally control flat light
 *
 * @author Jasem Mutlaq
 * @version 1.0
 */
class DustCap : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kstars.Ekos.DustCap")

  public:
    typedef enum { PARKING_IDLE, PARKING_OK, UNPARKING_OK, PARKING_BUSY, UNPARKING_BUSY, PARKING_ERROR } ParkingStatus;

    DustCap();
    virtual ~DustCap() override = default;

    /**
     * @defgroup DustCapDBusInterface Ekos DBus Interface - DustCap Interface
     * Ekos::DustCap interface provides basic DustCap operations.
     */

    /*@{*/

    /**
     * DBUS interface function.
     * If dust cap can park/unpark or is it just a light source?
     * @return True if park is supported, false otherwise
     */
    Q_SCRIPTABLE bool canPark();

    /**
     * DBUS interface function.
     * Park / Close dust cap
     * @return True if operation started/successful, false otherwise
     */
    Q_SCRIPTABLE bool park();

    /**
     * DBUS interface function.
     * UnPark / Open dust cap
     * @return True if operation started/successful, false otherwise
     */
    Q_SCRIPTABLE bool unpark();

    /**
     * DBUS interface function.
     * hasLight: Does the dust cap have a flat light source?
     * @return True if there if flat light, false othereise
     */
    Q_SCRIPTABLE bool hasLight();

    /**
     * DBUS interface function.
     * setLightEnabled: Turn on/off light box
     * @param enable If true, turn light on, otherwise turn light off
     * @return True if operation started/successful, false otherwise
     */
    Q_SCRIPTABLE bool setLightEnabled(bool enable);

    /**
     * DBUS interface function.
     * SetLight: Set light source brightness level
     * @return True if operation started/successful, false otherwise
     */
    Q_SCRIPTABLE bool setBrightness(uint16_t val);

    /**
     * DBUS interface function.
     * Get the dome park status
     */
    Q_SCRIPTABLE ParkingStatus getParkingStatus();

    /** @}*/

    /**
     * @brief setDustCap set the DustCap device
     * @param newDustCap pointer to DustCap device.
     */
    void setDustCap(ISD::GDInterface *newDustCap);

  private:
    // Devices needed for DustCap operation
    ISD::DustCap *currentDustCap { nullptr };
};
}
