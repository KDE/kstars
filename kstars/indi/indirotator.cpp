/*
    SPDX-FileCopyrightText: 2022 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "indirotator.h"

namespace ISD
{

bool Rotator::setAbsoluteAngle(double angle)
{
    auto nvp = getNumber("ABS_ROTATOR_ANGLE");

    if (!nvp)
        return false;

    if (std::abs(angle - nvp->at(0)->getValue()) < 0.001)
        return true;

    nvp->at(0)->setValue(angle);

    sendNewNumber(nvp);
    return true;
}

bool Rotator::setAbsoluteSteps(uint32_t steps)
{
    auto nvp = getNumber("ABS_ROTATOR_POSITION");

    if (!nvp)
        return false;

    if (steps == static_cast<uint32_t>(nvp->at(0)->getValue()))
        return true;

    nvp->at(0)->setValue(steps);

    sendNewNumber(nvp);
    return true;
}

bool Rotator::setReversed(bool enabled)
{
    auto svp = getSwitch("ROTATOR_REVERSE");

    if (!svp)
        return false;

    if ( (enabled && svp->sp[0].s == ISS_ON) ||
            (!enabled && svp->sp[1].s == ISS_ON))
        return true;

    svp->reset();
    svp->at(0)->setState(enabled ? ISS_ON : ISS_OFF);
    svp->at(1)->setState(enabled ? ISS_OFF : ISS_ON);

    sendNewSwitch(svp);
    return true;
}

void Rotator::registerProperty(INDI::Property prop)
{
    if (!strcmp(prop->getName(), "ABS_ROTATOR_ANGLE"))
        processNumber(prop.getNumber());
    if (!strcmp(prop->getName(), "ROTATOR_REVERSE"))
        processSwitch(prop.getSwitch());
}

void Rotator::processNumber(INumberVectorProperty *nvp)
{
    if (!strcmp(nvp->name, "ABS_ROTATOR_ANGLE"))
    {
        if (std::abs(nvp->np[0].value - m_AbsoluteAngle) > 0 || nvp->s != m_AbsoluteAngleState)
        {
            m_AbsoluteAngle = nvp->np[0].value;
            m_AbsoluteAngleState = nvp->s;
            emit newAbsoluteAngle(m_AbsoluteAngle, m_AbsoluteAngleState);
        }
    }
}

void Rotator::processSwitch(ISwitchVectorProperty *svp)
{
    if (!strcmp(svp->name, "ROTATOR_REVERSE"))
    {
        auto reverse = IUFindOnSwitchIndex(svp) == 0;
        if (m_Reversed != reverse)
        {
            m_Reversed = reverse;
            emit reverseToggled(m_Reversed);
        }
    }
}

}
