/*
    SPDX-FileCopyrightText: 2022 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "indiconcretedevice.h"

namespace ISD
{
/**
 * @class Rotator
 * Rotator class handles control of INDI Rotator devices.
 *
 * @author Jasem Mutlaq
 */
class Rotator : public ConcreteDevice
{
        Q_OBJECT

    public:
        explicit Rotator(GenericDevice *parent) : ConcreteDevice(parent) {}

        virtual void processNumber(INumberVectorProperty *nvp) override;
        virtual void processSwitch(ISwitchVectorProperty *svp) override;

    bool setAbsoluteAngle(double angle);
    bool setAbsoluteSteps(uint32_t steps);
    bool setReversed(bool enabled);

    bool isReversed() const {return m_Reversed;}
    double absoluteAngle() const {return m_AbsoluteAngle;}
    IPState absoluteAngleState() const {return m_AbsoluteAngleState;}

    signals:
        void newAbsoluteAngle(double value, IPState state);
        void reverseToggled(bool enabled);

    private:
        bool m_Reversed {false};
        double m_AbsoluteAngle {0};
        IPState m_AbsoluteAngleState {IPS_IDLE};
};
}
