/*
    SPDX-FileCopyrightText: 2023 Joseph McGee <joseph.mcgee@sbcglobal.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#ifndef CameraGainReadMode_H
#define CameraGainReadMode_H

#include <QAbstractItemModel>
#include <QVector>
#include "cameragainreadnoise.h"

QT_BEGIN_NAMESPACE
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
QT_END_NAMESPACE
#endif // CameraGainReadMode_H
