/*
    SPDX-FileCopyrightText: 2023 Joseph McGee <joseph.mcgee@sbcglobal.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/


#include "optimalexposuredetail.h"

namespace OptimalExposure
{


int OptimalExposureDetail::getSelectedGain() const
{
    return selectedGain;
}

void OptimalExposureDetail::setSelectedGain(int newSelectedGain)
{
    selectedGain = newSelectedGain;
}

double OptimalExposureDetail::getSubExposureTime() const
{
    return subExposureTime;
}

void OptimalExposureDetail::setSubExposureTime(double newSubExposureTime)
{
    subExposureTime = newSubExposureTime;
}

double OptimalExposureDetail::getExposurePollutionElectrons() const
{
    return exposurePollutionElectrons;
}

void OptimalExposureDetail::setExposurePollutionElectrons(double newExposurePollutionElectrons)
{
    exposurePollutionElectrons = newExposurePollutionElectrons;
}

double OptimalExposureDetail::getExposureShotNoise() const
{
    return exposureShotNoise;
}

void OptimalExposureDetail::setExposureShotNoise(double newExposureShotNoise)
{
    exposureShotNoise = newExposureShotNoise;
}

double OptimalExposureDetail::getExposureTotalNoise() const
{
    return exposureTotalNoise;
}

void OptimalExposureDetail::setExposureTotalNoise(double newExposureTotalNoise)
{
    exposureTotalNoise = newExposureTotalNoise;
}

const QVector<OptimalExposureStack> &OptimalExposureDetail::getStackSummary() const
{
    return stackSummary;
}

void OptimalExposureDetail::setStackSummary(const QVector<OptimalExposureStack> &newStackSummary)
{
    stackSummary = newStackSummary;
}


OptimalExposureDetail::OptimalExposureDetail(int selectedGain, double subExposureTime, double exposurePollutionElectrons,
        double exposureShotNoise, double exposureTotalNoise, const QVector<OptimalExposureStack> &stackSummary) :
    selectedGain(selectedGain),
    subExposureTime(subExposureTime),
    exposurePollutionElectrons(exposurePollutionElectrons),
    exposureShotNoise(exposureShotNoise),
    exposureTotalNoise(exposureTotalNoise),
    stackSummary(stackSummary)
{}

}
