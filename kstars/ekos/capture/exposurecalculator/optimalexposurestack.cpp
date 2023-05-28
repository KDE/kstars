/*
    SPDX-FileCopyrightText: 2023 Joseph McGee <joseph.mcgee@sbcglobal.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/


#include "optimalexposurestack.h"

namespace OptimalExposure
{


OptimalExposureStack::OptimalExposureStack()
{

}

int OptimalExposureStack::getPlannedTime() const
{
    return plannedTime;
}

void OptimalExposureStack::setPlannedTime(int newPlannedTime)
{
    plannedTime = newPlannedTime;
}

int OptimalExposureStack::getExposureCount() const
{
    return exposureCount;
}

void OptimalExposureStack::setExposureCount(int newExposureCount)
{
    exposureCount = newExposureCount;
}

int OptimalExposureStack::getStackTime() const
{
    return stackTime;
}

void OptimalExposureStack::setStackTime(int newStackTime)
{
    stackTime = newStackTime;
}

double OptimalExposureStack::getStackTotalNoise() const
{
    return stackTotalNoise;
}

void OptimalExposureStack::setStackTotalNoise(double newStackTotalNoise)
{
    stackTotalNoise = newStackTotalNoise;
}

OptimalExposureStack::OptimalExposureStack(int plannedTime, int exposureCount, int stackTime,
        double stackTotalNoise) : plannedTime(plannedTime),
    exposureCount(exposureCount),
    stackTime(stackTime),
    stackTotalNoise(stackTotalNoise)
{}
}
