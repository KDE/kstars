/*
    SPDX-FileCopyrightText: 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <memory>
#include <QTimer>

#include "indiconcretedevice.h"

namespace ISD
{
/**
 * @class Dome
 * Class handles control of INDI dome devices. Both open and closed loop (sensor feedback) domes are supported.
 *
 * @author Jasem Mutlaq
 */
class Dome : public ConcreteDevice
{
        Q_OBJECT
        Q_CLASSINFO("D-Bus Interface", "org.kde.kstars.INDI.Dome")
        Q_PROPERTY(bool canPark READ canPark)
        Q_PROPERTY(bool canAbsoluteMove READ canAbsoluteMove)
        Q_PROPERTY(bool canRelativeMove READ canRelativeMove)
        Q_PROPERTY(bool canAbort READ canAbort)
        Q_PROPERTY(bool isMoving READ isMoving)
        Q_PROPERTY(ISD::Dome::Status status READ status NOTIFY newStatus)
        Q_PROPERTY(ISD::Dome::ShutterStatus shutterStatus READ shutterStatus NOTIFY newShutterStatus)
        Q_PROPERTY(ISD::ParkStatus parkStatus READ parkStatus NOTIFY newParkStatus)
        Q_PROPERTY(double position READ position WRITE setPosition NOTIFY positionChanged)

    public:
        explicit Dome(GenericDevice *parent);

        // DBUS
        const QString &name()
        {
            return m_Parent->getDeviceName();
        }
        // DBus
        bool connected()
        {
            return m_Parent->isConnected();
        }

        typedef enum
        {
            DOME_IDLE,
            DOME_MOVING_CW,
            DOME_MOVING_CCW,
            DOME_TRACKING,
            DOME_PARKING,
            DOME_UNPARKING,
            DOME_PARKED,
            DOME_ERROR
        } Status;

        typedef enum
        {
            SHUTTER_UNKNOWN,
            SHUTTER_OPEN,
            SHUTTER_CLOSED,
            SHUTTER_OPENING,
            SHUTTER_CLOSING,
            SHUTTER_ERROR
        } ShutterStatus;

        typedef enum
        {
            DOME_CW,
            DOME_CCW
        } DomeDirection;

        typedef enum
        {
            MOTION_START,
            MOTION_STOP
        } DomeMotionCommand;


        void processSwitch(ISwitchVectorProperty *svp) override;
        void processNumber(INumberVectorProperty *nvp) override;
        void registerProperty(INDI::Property prop) override;

        Q_SCRIPTABLE bool canPark() const
        {
            return m_CanPark;
        }
        Q_SCRIPTABLE bool canAbsoluteMove() const
        {
            return m_CanAbsMove;
        }
        Q_SCRIPTABLE bool canRelativeMove() const
        {
            return m_CanRelMove;
        }
        Q_SCRIPTABLE bool canAbort() const
        {
            return m_CanAbort;
        }
        Q_SCRIPTABLE bool isParked() const
        {
            return m_ParkStatus == PARK_PARKED;
        }
        bool isMoving() const;

        /**
         * @return position Dome Azimuth position in degrees
         */
        Q_SCRIPTABLE double position() const;
        /**
         * @brief setPosition Set azimuth absolute position.
         * @param position Position in degrees 0 to +360
         * @return true if successful, false otherwise
         */
        Q_SCRIPTABLE bool setPosition(double position);

        bool setRelativePosition(double position);

        bool moveDome(DomeDirection dir, DomeMotionCommand operation);

        /** DBus Interface Function
         * @brief moveCW Start motion in clock-wise direction.
         * @return True if command is successful, false otherwise.
         */
        Q_SCRIPTABLE bool moveCW()
        {
            return moveDome(DOME_CW, MOTION_START);
        }

        /** DBus Interface Function
         * @brief moveCCW Start motion in counter-clock-wise direction.
         * @return True if command is successful, false otherwise.
         */
        Q_SCRIPTABLE bool moveCCW()
        {
            return moveDome(DOME_CCW, MOTION_START);
        }

        Q_SCRIPTABLE bool hasShutter() const
        {
            return m_HasShutter;
        }

        /**
         * @brief isRolloffRoof Do we have a roll off structure?
         * @return True if we do, false otherwise.
         */
        bool isRolloffRoof()
        {
            return (canAbsoluteMove() == false && canRelativeMove() == false);
        }

        // slaving
        bool isAutoSync();
        bool setAutoSync(bool activate);

        Status status() const
        {
            return m_Status;
        }
        ISD::ParkStatus parkStatus() const
        {
            return m_ParkStatus;
        }
        static const QString getStatusString (Status status, bool translated = true);

        ShutterStatus shutterStatus();
        ShutterStatus parseShutterStatus(ISwitchVectorProperty *svp);

        Q_SCRIPTABLE bool abort();
        Q_SCRIPTABLE bool park();
        Q_SCRIPTABLE bool unPark();

        Q_SCRIPTABLE bool controlShutter(bool open);
        bool openShutter()
        {
            return controlShutter(true);
        }
        bool closeShutter()
        {
            return controlShutter(false);
        }

    signals:
        void newStatus(ISD::Dome::Status status);
        void newParkStatus(ISD::ParkStatus status);
        void newShutterStatus(ISD::Dome::ShutterStatus status);
        void newAutoSyncStatus(bool enabled);
        void positionChanged(double degrees);

    private:
        ParkStatus m_ParkStatus { PARK_UNKNOWN };
        ShutterStatus m_ShutterStatus { SHUTTER_UNKNOWN };
        Status m_Status { DOME_IDLE };
        bool m_CanAbsMove { false };
        bool m_CanRelMove { false };
        bool m_CanPark { false };
        bool m_CanAbort { false };
        bool m_HasShutter { false };
        static const QList<const char *> domeStates;
};
}

Q_DECLARE_METATYPE(ISD::Dome::Status)
QDBusArgument &operator<<(QDBusArgument &argument, const ISD::Dome::Status &source);
const QDBusArgument &operator>>(const QDBusArgument &argument, ISD::Dome::Status &dest);

Q_DECLARE_METATYPE(ISD::Dome::ShutterStatus)
QDBusArgument &operator<<(QDBusArgument &argument, const ISD::Dome::ShutterStatus &source);
const QDBusArgument &operator>>(const QDBusArgument &argument, ISD::Dome::ShutterStatus &dest);
