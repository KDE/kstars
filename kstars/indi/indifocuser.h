/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "indiconcretedevice.h"

namespace ISD
{
/**
 * @class Focuser
 * Focuser class handles control of INDI focuser devices. Both relative and absolute focusers can be controlled.
 *
 * @author Jasem Mutlaq
 */

class Focuser : public ConcreteDevice
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

        explicit Focuser(GenericDevice *parent) : ConcreteDevice(parent) {}

        void registerProperty(INDI::Property prop) override;
        void processNumber(INumberVectorProperty *nvp) override;

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
        bool setMaxPosition(uint32_t steps);

    private:
        uint32_t m_maxPosition {0};
        int deviation = -1;

};
}
