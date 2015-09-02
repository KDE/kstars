/*  Ekos Dome interface
    Copyright (C) 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#ifndef DOME_H
#define DOME_H

#include <QtDBus/QtDBus>

#include "indi/indistd.h"
#include "indi/indidome.h"

namespace Ekos
{

/**
 *@class Dome
 *@short Supports basic dome functions
 *@author Jasem Mutlaq
 *@version 1.0
 */
class Dome : public QObject
{

    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kstars.Ekos.Dome")

public:
    Dome();
    ~Dome();

    /** @defgroup DomeDBusInterface Ekos DBus Interface - Dome Interface
     * Ekos::Dome interface provides advanced basic dome operations.
    */

    /*@{*/

    /** DBUS interface function.
     * Abort dome
     */
    Q_SCRIPTABLE bool abort();

    /** DBUS interface function.
     * Can dome park?
     */
    Q_SCRIPTABLE bool canPark();

    /** DBUS interface function.
     * Park dome
     */
    Q_SCRIPTABLE bool park();

    /** DBUS interface function.
     * Park dome
     */
    Q_SCRIPTABLE bool unpark();

    /** DBUS interface function.
     * Return true if dome is parked, false otherwise.
     */
    Q_SCRIPTABLE bool isParked();

    /** DBUS interface function.
     * Get Dome State. IDLE, BUSY, OK, or ALERT
     */
    Q_SCRIPTABLE IPState getDomeState();


    /** @}*/

    /**
     * @brief setDome set the dome device
     * @param newDome pointer to Dome device.
     */
    void setDome(ISD::GDInterface *newDome);

private:

    // Devices needed for Dome operation
    ISD::Dome *currentDome;

};

}

#endif  // Dome_H
