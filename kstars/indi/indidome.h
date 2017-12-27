/*  INDI Dome
    Copyright (C) 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#pragma once

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
    typedef enum { PARK_UNKNOWN, PARK_PARKED, PARK_PARKING, PARK_UNPARKING, PARK_UNPARKED } ParkStatus;

    explicit Dome(GDInterface *iPtr) : DeviceDecorator(iPtr) { dType = KSTARS_DOME; }

    void processSwitch(ISwitchVectorProperty *svp);
    void processText(ITextVectorProperty *tvp);
    void processNumber(INumberVectorProperty *nvp);
    void processLight(ILightVectorProperty *lvp);
    void registerProperty(INDI::Property *prop);

    DeviceFamily getType() { return dType; }

    bool canPark();
    bool isParked() { return parkStatus == PARK_PARKED; }
    bool isMoving();

  public slots:
    bool Abort();
    bool Park();
    bool UnPark();

  private:
    ParkStatus parkStatus = PARK_UNKNOWN;
};
}
