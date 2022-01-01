/*  Ekos commands for the capture module
    SPDX-FileCopyrightText: Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "capturecommandprocessor.h"

#include "indi/indistd.h"

namespace Ekos
{
CaptureCommandProcessor::CaptureCommandProcessor()
{

}

void CaptureCommandProcessor::setRotatorAngle(double *rawAngle)
{
 if (activeRotator != nullptr)
     activeRotator->runCommand(INDI_SET_ROTATOR_ANGLE, rawAngle);
}

void CaptureCommandProcessor::setCCDTemperature(double temp)
{
    if (activeCCD != nullptr)
        activeCCD->setTemperature(temp);
}

void CaptureCommandProcessor::enableCCDBatchMode(bool enable)
{
    if (activeChip != nullptr)
        activeChip->setBatchMode(enable);
}
} // namespace
