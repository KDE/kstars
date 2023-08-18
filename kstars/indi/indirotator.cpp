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

    if (std::abs(angle - nvp->at(0)->getValue()) < 0.01)
        return true;

    nvp->at(0)->setValue(angle);

    sendNewProperty(nvp);
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

    sendNewProperty(nvp);
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

    sendNewProperty(svp);
    return true;
}

void Rotator::registerProperty(INDI::Property prop)
{
    if (prop.isNameMatch("ABS_ROTATOR_ANGLE"))
        processNumber(prop);
    else if (prop.isNameMatch("ROTATOR_REVERSE"))
        processSwitch(prop);
}

void Rotator::processNumber(INDI::Property prop)
{
    auto nvp = prop.getNumber();
    if (nvp->isNameMatch("ABS_ROTATOR_ANGLE"))
    {
        if (std::abs(nvp->at(0)->getValue() - m_AbsoluteAngle) > 0 || nvp->getState() != m_AbsoluteAngleState)
        {
            m_AbsoluteAngle = nvp->at(0)->getValue();
            m_AbsoluteAngleState = nvp->getState();
            emit newAbsoluteAngle(m_AbsoluteAngle, m_AbsoluteAngleState);
        }
    }
}

void Rotator::processSwitch(INDI::Property prop)
{
    auto svp = prop.getSwitch();
    if (svp->isNameMatch("ROTATOR_REVERSE"))
    {
        auto reverse = svp->findOnSwitchIndex() == 0;
        if (m_Reversed != reverse)
        {
            m_Reversed = reverse;
            emit reverseToggled(m_Reversed);
        }
    }
}

}
