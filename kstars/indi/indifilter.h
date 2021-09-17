/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
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
