/*
    SPDX-FileCopyrightText: 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <memory>
#include <QTimer>

#include "indistd.h"

namespace ISD
{
/**
 * @class Dome
 * Class handles control of INDI dome devices. Both open and closed loop (sensor feedback) domes are supported.
 *
 * @author Jasem Mutlaq
 */
class Dome : public DeviceDecorator
{
        Q_OBJECT

public:
    explicit Dome(GDInterface *iPtr);
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
        void processText(ITextVectorProperty *tvp) override;
        void processNumber(INumberVectorProperty *nvp) override;
        void processLight(ILightVectorProperty *lvp) override;
        void registerProperty(INDI::Property prop) override;

        DeviceFamily getType() override
        {
            return dType;
        }

        bool canPark() const
        {
            return m_CanPark;
        }
        bool canAbsMove() const
        {
            return m_CanAbsMove;
        }
        bool canRelMove() const
        {
            return m_CanRelMove;
        }
        bool canAbort() const
        {
            return m_CanAbort;
        }
        bool isParked() const
        {
            return m_ParkStatus == PARK_PARKED;
        }
        bool isMoving() const;

        double azimuthPosition() const;
        bool setAzimuthPosition(double position);
        bool setRelativePosition(double position);

        bool moveDome(DomeDirection dir, DomeMotionCommand operation);

        bool hasShutter() const
        {
            return m_HasShutter;
        }

        // slaving
        bool isAutoSync();
        bool setAutoSync(bool activate);

        Status status() const
        {
            return m_Status;
        }
        static const QString getStatusString (Status status);

        ShutterStatus shutterStatus();
        ShutterStatus shutterStatus(ISwitchVectorProperty *svp);

public slots:
        bool Abort();
        bool Park();
        bool UnPark();
        bool ControlShutter(bool open);

    signals:
        void newStatus(Status status);
        void newParkStatus(ParkStatus status);
        void newShutterStatus(ShutterStatus status);
        void newAutoSyncStatus(bool enabled);
        void azimuthPositionChanged(double Az);
        void ready();

    private:
        ParkStatus m_ParkStatus { PARK_UNKNOWN };
        ShutterStatus m_ShutterStatus { SHUTTER_UNKNOWN };
        Status m_Status { DOME_IDLE };
        bool m_CanAbsMove { false };
        bool m_CanRelMove { false };
        bool m_CanPark { false };
        bool m_CanAbort { false };
        bool m_HasShutter { false };
        std::unique_ptr<QTimer> readyTimer;
};
}

Q_DECLARE_METATYPE(ISD::Dome::Status)
QDBusArgument &operator<<(QDBusArgument &argument, const ISD::Dome::Status &source);
const QDBusArgument &operator>>(const QDBusArgument &argument, ISD::Dome::Status &dest);

