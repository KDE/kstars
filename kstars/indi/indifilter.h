/*  INDI Filter
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#ifndef INDIFILTER_H
#define INDIFILTER_H

#include "indistd.h"

namespace ISD
{

/**
 * @class Filter class handles control of INDI Filter devices.
 *
 * @author Jasem Mutlaq
 */
class Filter : public DeviceDecorator
{
    Q_OBJECT

public:
    Filter(GDInterface *iPtr) : DeviceDecorator(iPtr) { dType = KSTARS_FILTER;}

    void processSwitch(ISwitchVectorProperty *svp);
    void processText(ITextVectorProperty* tvp);
    void processNumber(INumberVectorProperty *nvp);
    void processLight(ILightVectorProperty *lvp);

    DeviceFamily getType() { return dType;}

};

}

#endif // INDIFILTER_H
