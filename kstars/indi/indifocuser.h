/*  INDI Focuser
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
 * @class Focuser
 * Focuser class handles control of INDI focuser devices. Both relative and absolute focusers can be controlled.
 *
 * @author Jasem Mutlaq
 */

class Focuser : public DeviceDecorator
{
        Q_OBJECT


    public:
        enum FocusDirection
        {
            FOCUS_INWARD,
            FOCUS_OUTWARD
        };
        enum FocusDeviation
        {
            NIKONZ6
        };

        explicit Focuser(GDInterface *iPtr) : DeviceDecorator(iPtr)
        {
            dType = KSTARS_FOCUSER;
        }

        void registerProperty(INDI::Property *prop) override;
        void processSwitch(ISwitchVectorProperty *svp) override;
        void processText(ITextVectorProperty *tvp) override;
        void processNumber(INumberVectorProperty *nvp) override;
        void processLight(ILightVectorProperty *lvp) override;

        DeviceFamily getType() override
        {
            return dType;
        }

        bool focusIn();
        bool focusOut();
        bool stop();
        bool moveByTimer(int msecs);
        bool moveAbs(int steps);
        bool moveRel(int steps);

        bool canAbsMove();
        bool canRelMove();
        bool canTimerMove();
        bool canManualFocusDriveMove();
        double getLastManualFocusDriveValue();

        bool hasBacklash();
        bool hasDeviation();
        bool setBacklash(int32_t steps);
        int32_t getBacklash();

        bool getFocusDirection(FocusDirection *dir);

        // Max Travel
        uint32_t getmaxPosition()
        {
            return m_maxPosition;
        }
        bool setmaxPosition(uint32_t steps);

    private:
        uint32_t m_maxPosition {0};
        int deviation = -1;

};
}
