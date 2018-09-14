/*  INDI Dome
    Copyright (C) 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#pragma once

#include <memory>
#include <QTimer>

#include "indistd.h"

namespace ISD
{
/**
 * @class Dome
 * Focuser class handles control of INDI dome devices. Both open and closed loop (senor feedback) domes are supported.
 *
 * @author Jasem Mutlaq
 */
class Dome : public DeviceDecorator
{
    Q_OBJECT

  public:    
    explicit Dome(GDInterface *iPtr);
    typedef enum {
        DOME_IDLE,
        DOME_MOVING,
        DOME_TRACKING,
        DOME_PARKING,
        DOME_UNPARKING,
        DOME_PARKED,
        DOME_ERROR
    } Status;

    void processSwitch(ISwitchVectorProperty *svp);
    void processText(ITextVectorProperty *tvp);
    void processNumber(INumberVectorProperty *nvp);
    void processLight(ILightVectorProperty *lvp);
    void registerProperty(INDI::Property *prop);

    DeviceFamily getType() { return dType; }

    bool canPark() { return m_CanPark; }
    bool canAbsMove() { return m_CanAbsMove; }
    bool isParked() { return m_ParkStatus == PARK_PARKED; }
    bool isMoving();    

    double azimuthPosition();
    bool setAzimuthPosition(double position);

  public slots:
    bool Abort();
    bool Park();
    bool UnPark();

  signals:
    void newStatus(Status status);
    void newParkStatus(ParkStatus status);
    void azimuthPositionChanged(double Az);
    void ready();

  private:
    ParkStatus m_ParkStatus = PARK_UNKNOWN;
    Status m_Status = DOME_IDLE;
    bool m_CanAbsMove { false };
    bool m_CanPark { false };
    std::unique_ptr<QTimer> readyTimer;
};
}

Q_DECLARE_METATYPE(ISD::Dome::Status)
QDBusArgument &operator<<(QDBusArgument &argument, const ISD::Dome::Status& source);
const QDBusArgument &operator>>(const QDBusArgument &argument, ISD::Dome::Status &dest);

