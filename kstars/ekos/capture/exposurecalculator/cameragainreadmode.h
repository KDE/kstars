/*
    SPDX-FileCopyrightText: 2023 Joseph McGee <joseph.mcgee@sbcglobal.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QAbstractItemModel>
#include <QVector>
#include "cameragainreadnoise.h"

namespace OptimalExposure
{
class CameraGainReadMode
{
    public:
        CameraGainReadMode() {}
        CameraGainReadMode(int CameraGainReadModeNumber, const QString &CameraGainReadModeName,
                           const QVector<OptimalExposure::CameraGainReadNoise> &CameraGainReadNoiseVector);

        int getCameraGainReadModeNumber() const;
        void setCameraGainReadModeNumber(int newCameraGainReadModeNumber);

        const QString &getCameraGainReadModeName() const;
        void setCameraGainReadModeName(const QString &newCameraGainReadModeName);

        const QVector<OptimalExposure::CameraGainReadNoise> &getCameraGainReadNoiseVector() const;
        void setCameraGainReadNoiseVector(const QVector<OptimalExposure::CameraGainReadNoise> &newCameraGainReadNoiseVector);

    private:
        int CameraGainReadModeNumber;
        QString CameraGainReadModeName;
        QVector<OptimalExposure::CameraGainReadNoise> CameraGainReadNoiseVector;
};
}
