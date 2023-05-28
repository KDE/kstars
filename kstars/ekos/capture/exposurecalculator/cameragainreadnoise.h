/*
    SPDX-FileCopyrightText: 2023 Joseph McGee <joseph.mcgee@sbcglobal.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QAbstractItemModel>

namespace OptimalExposure
{

class CameraGainReadNoise
{

    public:
        CameraGainReadNoise() {}
        CameraGainReadNoise(int gain, double readNoise);

        int getGain();
        void setGain(int newGain);
        double getReadNoise();
        void setReadNoise(double newReadNoise);

    private:
        int gain = 0;
        double readNoise = 0.0;

};

}





