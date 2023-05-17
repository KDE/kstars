/*
    SPDX-FileCopyrightText: 2023 Joseph McGee <joseph.mcgee@sbcglobal.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#ifndef OPTIMALEXPOSURESTACK_H
#define OPTIMALEXPOSURESTACK_H
#include <QAbstractItemModel>

QT_BEGIN_NAMESPACE
namespace OptimalExposure
{
class OptimalExposureStack
{
    public:
        OptimalExposureStack();
        OptimalExposureStack(int plannedTime, int exposureCount, int stackTime, double stackTotalNoise);

        int getPlannedTime() const;
        void setPlannedTime(int newPlannedTime);
        int getExposureCount() const;
        void setExposureCount(int newExposureCount);
        int getStackTime() const;
        void setStackTime(int newStackTime);
        double getStackTotalNoise() const;
        void setStackTotalNoise(double newStackTotalNoise);

    private:
        int plannedTime;
        int exposureCount;
        int stackTime;
        double stackTotalNoise;
};
}
QT_END_NAMESPACE



#endif // OPTIMALEXPOSURESTACK_H
