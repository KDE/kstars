/*
    SPDX-FileCopyrightText: 2023 Joseph McGee <joseph.mcgee@sbcglobal.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#ifndef OPTIMALEXPOSUREDETAIL_H
#define OPTIMALEXPOSUREDETAIL_H
#include <QAbstractItemModel>
#include <QVector>
#include "optimalexposurestack.h"

QT_BEGIN_NAMESPACE
namespace OptimalExposure
{
class OptimalExposureDetail
{
    public:
        OptimalExposureDetail() {}
        OptimalExposureDetail(int selectedGain, double subExposureTime, double exposurePollutionElectrons, double exposureShotNoise,
                              double exposureTotalNoise, const QVector<OptimalExposureStack> &stackSummary);

        int getSelectedGain() const;
        void setSelectedGain(int newSelectedGain);
        double getSubExposureTime() const;
        void setSubExposureTime(double newSubExposureTime);
        double getExposurePollutionElectrons() const;
        void setExposurePollutionElectrons(double newExposurePollutionElectrons);
        double getExposureShotNoise() const;
        void setExposureShotNoise(double newExposureShotNoise);
        double getExposureTotalNoise() const;
        void setExposureTotalNoise(double newExposureTotalNoise);
        const QVector<OptimalExposureStack> &getStackSummary() const;
        void setStackSummary(const QVector<OptimalExposureStack> &newStackSummary);


    private:
        int selectedGain;
        double subExposureTime;
        double exposurePollutionElectrons;
        double exposureShotNoise;
        double exposureTotalNoise;
        QVector<OptimalExposureStack> stackSummary ;
};
}
QT_END_NAMESPACE

#endif // OPTIMALEXPOSUREDETAIL_H
