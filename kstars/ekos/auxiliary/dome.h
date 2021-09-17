/*
    SPDX-FileCopyrightText: 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
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
        Q_PROPERTY(bool canRelativeMove READ canRelativeMove)
        Q_PROPERTY(bool isMoving READ isMoving)
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
         * Can dome move to an relative azimuth position?
         */
        Q_SCRIPTABLE bool canRelativeMove();
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

        Q_SCRIPTABLE bool isRolloffRoof();

        Q_SCRIPTABLE double azimuthPosition();
        Q_SCRIPTABLE void setAzimuthPosition(double position);
        Q_SCRIPTABLE void setRelativePosition(double position);

        Q_SCRIPTABLE bool moveDome(bool moveCW, bool start);


        Q_SCRIPTABLE bool isAutoSync();
        Q_SCRIPTABLE bool setAutoSync(bool activate);

        Q_SCRIPTABLE bool hasShutter();
        Q_SCRIPTABLE bool controlShutter(bool open);

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
        //void setTelescope(ISD::GDInterface *newTelescope);

        void removeDevice(ISD::GDInterface *device);

        ISD::Dome::Status status()
        {
            return currentDome->status();
        }
        ISD::Dome::ShutterStatus shutterStatus()
        {
            return currentDome->shutterStatus();
        }
        ISD::ParkStatus parkStatus()
        {
            return m_ParkStatus;
        }

    signals:
        void newStatus(ISD::Dome::Status status);
        void newParkStatus(ISD::ParkStatus status);
        void newShutterStatus(ISD::Dome::ShutterStatus status);
        void newAutoSyncStatus(bool enabled);
        void azimuthPositionChanged(double position);
        void ready();
        // Signal when the underlying ISD::Dome signals a Disconnected()
        void disconnected();

    private:
        // Devices needed for Dome operation
        ISD::Dome *currentDome { nullptr };
        ISD::ParkStatus m_ParkStatus { ISD::PARK_UNKNOWN };
        ISD::Dome::ShutterStatus m_ShutterStatus { ISD::Dome::SHUTTER_UNKNOWN };
        void setStatus(ISD::Dome::Status status);
};

}
