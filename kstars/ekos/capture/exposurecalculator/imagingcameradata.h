/*
    SPDX-FileCopyrightText: 2023 Joseph McGee <joseph.mcgee@sbcglobal.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#ifndef IMAGINGCAMERADATA_H
#define IMAGINGCAMERADATA_H

#include <QAbstractItemModel>
#include <QVector>
#include "cameragainreadmode.h"
#include "cameragainreadnoise.h"

QT_BEGIN_NAMESPACE
namespace OptimalExposure
{

typedef enum { SENSORTYPE_MONOCHROME, SENSORTYPE_COLOR } SensorType;
// GAIN_SELECTION_TYPE_FIXED is for future development of CCD cameras in which read-noise does not vary with gain.
typedef enum { GAIN_SELECTION_TYPE_NORMAL, GAIN_SELECTION_TYPE_ISO_DISCRETE, GAIN_SELECTION_TYPE_FIXED } GainSelectionType;


class ImagingCameraData
{

    public:
        ImagingCameraData() {}
        ImagingCameraData(const QString &cameraId, OptimalExposure::SensorType sensorType,
                          OptimalExposure::GainSelectionType gainSelectionType, const QVector<int> &gainSelectionRange,
                          const QVector<OptimalExposure::CameraGainReadMode> &CameraGainReadModeVector, double cameraPixelSize, double cameraQuantumEfficiency);

        int getDataClassVersion();
        void setDataClassVersion(int newDataClassVersion);

        QString getCameraId();
        void setCameraId(const QString newCameraId);

        SensorType getSensorType() const;
        void setSensorType(SensorType newSensorType);

        OptimalExposure::GainSelectionType getGainSelectionType() const;
        void setGainSelectionType(OptimalExposure::GainSelectionType newGainSelectionType);

        int getGainMin();
        int getGainMax();

        QVector<CameraGainReadMode> getCameraGainReadModeVector();
        void setCameraGainReadModeVector(QVector<CameraGainReadMode> newCameraGainReadModeVector);

        QVector<int> getGainSelectionRange();
        void setGainSelectionRange(QVector<int> newGainSelectionRange);
        
        double getCameraPixelSize();
        void setCameraPixelSize(double newCameraPixelSize);
        double getCameraQuantumEfficiency();
        void setCameraQuantumEfficiency(double newCameraQuantumEfficiency);



    private:
        QString cameraId;
        int dataClassVersion;

        OptimalExposure::SensorType sensorType;
        OptimalExposure::GainSelectionType gainSelectionType;

        // For GAIN_SELECTION_TYPE_NORMAL gainSelection holds only the min and max gains.
        // For GAIN_SELECTION_TYPE_ISO_DISCRETE, gainSelection hold the discrete values.
        // For GAIN_SELECTION_TYPE_FIXED the gainSelection will not be populated
        QVector<int> gainSelectionRange;
        QVector<OptimalExposure::CameraGainReadMode> CameraGainReadModeVector;
		
		double cameraPixelSize;
		double cameraQuantumEfficiency;

};
}
QT_END_NAMESPACE
#endif // IMAGINGCAMERADATA_H
