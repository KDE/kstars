/*
    SPDX-FileCopyrightText: 2023 Joseph McGee <joseph.mcgee@sbcglobal.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/


#include "cameragainreadmode.h"
#include "imagingcameradata.h"
#include <QAbstractItemModel>
#include <QVector>

namespace OptimalExposure
{

int OptimalExposure::ImagingCameraData::getDataClassVersion()
{
    return dataClassVersion;
}

void OptimalExposure::ImagingCameraData::setDataClassVersion(int newClassVersion)
{
    dataClassVersion = newClassVersion;
}

QString OptimalExposure::ImagingCameraData::getCameraId()
{
    return cameraId;
}

void OptimalExposure::ImagingCameraData::setCameraId(QString newCameraId)
{
    cameraId = newCameraId;
}

OptimalExposure::SensorType OptimalExposure::ImagingCameraData::getSensorType() const
{
    return sensorType;
}

void OptimalExposure::ImagingCameraData::setSensorType(SensorType newSensorType)
{
    sensorType = newSensorType;
}

OptimalExposure::GainSelectionType ImagingCameraData::getGainSelectionType() const
{
    return gainSelectionType;
}

void ImagingCameraData::setGainSelectionType(OptimalExposure::GainSelectionType newGainSelectionType)
{
    gainSelectionType = newGainSelectionType;
}

int OptimalExposure::ImagingCameraData::getGainMin()
{
    int gainMin = 0;
    if(getGainSelectionRange().count() > 0) gainMin = getGainSelectionRange()[0];
    return gainMin;
}

int OptimalExposure::ImagingCameraData::getGainMax()
{
    int gainMax = 0;
    if(getGainSelectionRange().count() > 0) gainMax = getGainSelectionRange()[getGainSelectionRange().count() - 1];
    return gainMax;
}

void ImagingCameraData::setGainSelectionRange(QVector<int> newGainSelectionRange)
{
    gainSelectionRange = newGainSelectionRange;
}



QVector<int> ImagingCameraData::getGainSelectionRange()
{
    return gainSelectionRange;
}

void OptimalExposure::ImagingCameraData::setCameraGainReadModeVector(QVector<CameraGainReadMode>
        newCameraGainReadModeVector)
{
    CameraGainReadModeVector = newCameraGainReadModeVector;
}

QVector<OptimalExposure::CameraGainReadMode> OptimalExposure::ImagingCameraData::getCameraGainReadModeVector()
{
    return CameraGainReadModeVector;
}

void OptimalExposure::ImagingCameraData::setCameraPixelSize(double newCameraPixelSize)
{
	cameraPixelSize = newCameraPixelSize;
}

double OptimalExposure::ImagingCameraData::getCameraPixelSize()
{
	return cameraPixelSize;
}

void OptimalExposure::ImagingCameraData::setCameraQuantumEfficiency(double newCameraQuantumEfficiency)
{
	cameraQuantumEfficiency = newCameraQuantumEfficiency;
}

double OptimalExposure::ImagingCameraData::getCameraQuantumEfficiency()
{
	return cameraQuantumEfficiency;
}


ImagingCameraData::ImagingCameraData(const QString &cameraId, SensorType sensorType, GainSelectionType gainSelectionType,
                                     const QVector<int> &gainSelectionRange, const QVector<CameraGainReadMode> &CameraGainReadModeVector, double cameraPixelSize,
                                     double cameraQuantumEfficiency) : cameraId(cameraId),
    sensorType(sensorType),
    gainSelectionType(gainSelectionType),
    gainSelectionRange(gainSelectionRange),
    CameraGainReadModeVector(CameraGainReadModeVector),
    cameraPixelSize(cameraPixelSize),
    cameraQuantumEfficiency(cameraQuantumEfficiency)
{}

}
