/*
    SPDX-FileCopyrightText: 2023 Joseph McGee <joseph.mcgee@sbcglobal.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/


#include <QLoggingCategory>
#include <QAbstractItemModel>
#include "cameragainreadnoise.h"

#include <ekos_capture_debug.h>

int OptimalExposure::CameraGainReadNoise::getGain()
{
    return gain;
}

void OptimalExposure::CameraGainReadNoise::setGain(int newGain)
{
    gain = newGain;
}

double OptimalExposure::CameraGainReadNoise::getReadNoise()
{
    return readNoise;
}

void OptimalExposure::CameraGainReadNoise::setReadNoise(double newReadNoise)
{
    readNoise = newReadNoise;
}


namespace OptimalExposure
{
CameraGainReadNoise::CameraGainReadNoise(int gain, double readNoise) : gain(gain), readNoise(readNoise)
{
    // qCInfo(KSTARS_EKOS_CAPTURE) << "CameraGainReadNoise constructor: " << gain << " " << readNoise;
}
}
