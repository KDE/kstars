/*  INDI Filter
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

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
 * @class Filter
 * Filter class handles control of INDI Filter devices.
 *
 * @author Jasem Mutlaq
 */
class Filter : public DeviceDecorator
{
        Q_OBJECT

    public:
        explicit Filter(GDInterface *iPtr) : DeviceDecorator(iPtr)
        {
            dType = KSTARS_FILTER;
        }

        void processSwitch(ISwitchVectorProperty *svp) override;
        void processText(ITextVectorProperty *tvp) override;
        void processNumber(INumberVectorProperty *nvp) override;
        void processLight(ILightVectorProperty *lvp) override;

        DeviceFamily getType() override
        {
            return dType;
        }
};
}
