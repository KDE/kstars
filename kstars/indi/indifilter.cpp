/*  INDI Filter
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */


#include "indifilter.h"

namespace ISD
{


void Filter::processLight(ILightVectorProperty *lvp)
{
    DeviceDecorator::processLight(lvp);
}

void Filter::processNumber(INumberVectorProperty *nvp)
{
    DeviceDecorator::processNumber(nvp);
}

void Filter::processSwitch(ISwitchVectorProperty *svp)
{
    DeviceDecorator::processSwitch(svp);

}

void Filter::processText(ITextVectorProperty *tvp)
{
    DeviceDecorator::processText(tvp);
}

}
