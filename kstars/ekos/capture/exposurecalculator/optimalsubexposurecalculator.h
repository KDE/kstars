/*
    SPDX-FileCopyrightText: 2023 Joseph McGee <joseph.mcgee@sbcglobal.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#ifndef OPTIMALSUBEXPOSURECALCULATOR_H
#define OPTIMALSUBEXPOSURECALCULATOR_H
#include <QAbstractItemModel>
#include <QVector>
#include "imagingcameradata.h"
#include "calculatedgainsubexposuretime.h"
#include "cameraexposureenvelope.h"
#include "optimalexposuredetail.h"

QT_BEGIN_NAMESPACE
namespace OptimalExposure
{


class OptimalSubExposureCalculator
{
    public:
        OptimalSubExposureCalculator();
        OptimalSubExposureCalculator(double aNoiseTolerance, double aSkyQuality, double aFocalRatio, double aFilterCompensation,
                                     ImagingCameraData &aCalculationImagingCameraData);

        CameraExposureEnvelope calculateCameraExposureEnvelope();
        // OptimalExposureDetail calculateSubExposureDetail(int aSelectedGainValue);
        OptimalExposureDetail calculateSubExposureDetail();

        double getANoiseTolerance();
        void setANoiseTolerance(double newNoiseTolerance);

        double getASkyQuality();
        void setASkyQuality(double newSkyQuality);

        double getAFocalRatio();
        void setAFocalRatio(double newFocalRatio);

        double getAFilterCompensation();
        void setAFilterCompensation(double newFilterCompensation);

        ImagingCameraData &getImagingCameraData();
        void setImagingCameraData(ImagingCameraData &aNewCalculationImagingCameraData);


        int getASelectedGain();
        void setASelectedGain(int newSelectedGain);

        int getASelectedCameraReadMode() const;
        void setASelectedCameraReadMode(int aNewSelectedCameraReadMode);

    protected:
        double aNoiseTolerance;
        double aSkyQuality;
        double aFocalRatio;
        double aFilterCompensation;
        int aSelectedGain = 0;
        int aSelectedCameraReadMode = 0;
        ImagingCameraData anImagingCameraData;


    private:
        double calculateCFactor(double noiseTolerance);
        double calculateLightPollutionElectronBaseRate(double skyQuality);
        double calculateLightPolutionForOpticFocalRatio(double lightPollutionElectronBaseRate, double aFocalRatio,
                double AFilterCompensation);
        QVector<CalculatedGainSubExposureTime> calculateGainSubExposureVector(double cFactor,
                double lightPollutionForOpticFocalRatio);


};

}
QT_END_NAMESPACE

#endif // OPTIMALSUBEXPOSURECALCULATOR_H
