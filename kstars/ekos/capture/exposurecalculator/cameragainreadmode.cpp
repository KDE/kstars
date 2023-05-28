/*
    SPDX-FileCopyrightText: 2023 Joseph McGee <joseph.mcgee@sbcglobal.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/


#include "cameragainreadmode.h"
#include "cameragainreadnoise.h"
#include <QAbstractItemModel>
#include <QVector>

namespace OptimalExposure
{


int CameraGainReadMode::getCameraGainReadModeNumber() const
{
    return CameraGainReadModeNumber;
}

void CameraGainReadMode::setCameraGainReadModeNumber(int newCameraGainReadModeNumber)
{
    CameraGainReadModeNumber = newCameraGainReadModeNumber;
}

const QString &CameraGainReadMode::getCameraGainReadModeName() const
{
    return CameraGainReadModeName;
}

void CameraGainReadMode::setCameraGainReadModeName(const QString &newCameraGainReadModeName)
{
    CameraGainReadModeName = newCameraGainReadModeName;
}

const QVector<OptimalExposure::CameraGainReadNoise> &CameraGainReadMode::getCameraGainReadNoiseVector() const
{
    return CameraGainReadNoiseVector;
}

void CameraGainReadMode::setCameraGainReadNoiseVector(const QVector<OptimalExposure::CameraGainReadNoise>
        &newCameraGainReadNoiseVector)
{
    CameraGainReadNoiseVector = newCameraGainReadNoiseVector;
}

CameraGainReadMode::CameraGainReadMode(int CameraGainReadModeNumber, const QString &CameraGainReadModeName,
                                       const QVector<CameraGainReadNoise> &CameraGainReadNoiseVector) : CameraGainReadModeNumber(CameraGainReadModeNumber),
    CameraGainReadModeName(CameraGainReadModeName),
    CameraGainReadNoiseVector(CameraGainReadNoiseVector)
{}

}
