/*  INDI Weather
    Copyright (C) 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#ifndef INDIWEATHER_H
#define INDIWEATHER_H

#include "indistd.h"

namespace ISD
{

/**
 * @class Weather
 * Focuser class handles control of INDI Weather devices. It reports overall state and the value of each parameter
 *
 * @author Jasem Mutlaq
 */

class Weather : public DeviceDecorator
{
    Q_OBJECT

public:
    Weather(GDInterface *iPtr) : DeviceDecorator(iPtr) { dType = KSTARS_WEATHER;}

    void processSwitch(ISwitchVectorProperty *svp);
    void processText(ITextVectorProperty* tvp);
    void processNumber(INumberVectorProperty *nvp);
    void processLight(ILightVectorProperty *lvp);

    DeviceFamily getType() { return dType;}

    IPState getWeatherStatus();

    uint16_t getUpdatePeriod();

};

}

#endif // INDIWEATHER
