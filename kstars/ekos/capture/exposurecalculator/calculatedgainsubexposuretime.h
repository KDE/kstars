/*
    SPDX-FileCopyrightText: 2023 Joseph McGee <joseph.mcgee@sbcglobal.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#ifndef CALCULATEDGAINSUBEXPOSURETIME_H
#define CALCULATEDGAINSUBEXPOSURETIME_H

#include <QAbstractItemModel>
QT_BEGIN_NAMESPACE
namespace OptimalExposure
{

class CalculatedGainSubExposureTime
{

    public:
        CalculatedGainSubExposureTime() {}
        CalculatedGainSubExposureTime(int subExposureGain, double subExposureTime);

        int getSubExposureGain();
        void setSubExposureGain(int newSubExposureGain);
        double getSubExposureTime();
        void setSubExposureTime(double newSubExposureTime);

    private:
        int subExposureGain = 0;
        double subExposureTime = 0.0;

};

}
QT_END_NAMESPACE

#endif // CALCULATEDGAINSUBEXPOSURETIME_H
