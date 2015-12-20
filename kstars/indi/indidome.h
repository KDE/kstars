/*  INDI Dome
    Copyright (C) 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#ifndef INDIDOME_H
#define INDIDOME_H

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
    Dome(GDInterface *iPtr) : DeviceDecorator(iPtr) { dType = KSTARS_DOME;}

    void processSwitch(ISwitchVectorProperty *svp);
    void processText(ITextVectorProperty* tvp);
    void processNumber(INumberVectorProperty *nvp);
    void processLight(ILightVectorProperty *lvp);

    DeviceFamily getType() { return dType;}

    bool canPark();
    bool isParked();

public slots:
    bool Abort();
    bool Park();
    bool UnPark();

};

}

#endif // INDIDOME_H
