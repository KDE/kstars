/*
    SPDX-FileCopyrightText: 2023 Joseph McGee <joseph.mcgee@sbcglobal.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/


#include <QLoggingCategory>
#include "calculatedgainsubexposuretime.h"
#include <QAbstractItemModel>

#include <ekos_capture_debug.h>

int OptimalExposure::CalculatedGainSubExposureTime::getSubExposureGain()
{
    return subExposureGain;
}

void OptimalExposure::CalculatedGainSubExposureTime::setSubExposureGain(int newSubExposureGain)
{
    subExposureGain = newSubExposureGain;
}

double OptimalExposure::CalculatedGainSubExposureTime::getSubExposureTime()
{
    return subExposureTime;
}

void OptimalExposure::CalculatedGainSubExposureTime::setSubExposureTime(double newSubExposureTime)
{
    subExposureTime = newSubExposureTime;
}



namespace OptimalExposure
{
CalculatedGainSubExposureTime::CalculatedGainSubExposureTime(int subExposureGain,
        double subExposureTime) : subExposureGain(subExposureGain), subExposureTime(subExposureTime)
{
    // qCInfo(KSTARS_EKOS_CAPTURE) << "CalculatedGainSubExposureTime constructor: "
    // << subExposureGain << " " << subExposureTime;
}

}
