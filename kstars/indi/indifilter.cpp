/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
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
