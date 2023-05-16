/*
    SPDX-FileCopyrightText: 2022 Toni Schriber
    SPDX-License-Identifier: GPL-2.0-or-later
*/


#pragma once

#include "indi/indistd.h"
#include "indi/indilistener.h"
#include "indi/indimount.h"

class RotatorUtils : public QObject
{
        Q_OBJECT

    public:
        RotatorUtils();
        ~RotatorUtils();

        void initRotatorUtils();
        static void   setImageFlip(bool state);
        static bool   checkImageFlip();
        static double calcRotatorAngle(double PositionAngle);
        static double calcCameraAngle(double RotatorAngle, bool flippedImage);
        static double calcOffsetAngle(double RotatorAngle, double PositionAngle);
        static void   updateOffset(double Angle);
        static void   setImagePierside(ISD::Mount::PierSide ImgPierside);
        ISD::Mount::PierSide getMountPierside();
        static double DiffPA(double diff);

    private:
        static ISD::Mount::PierSide m_CalPierside;
        static ISD::Mount::PierSide m_ImgPierside;
        static double m_Offset;
        static bool m_flippedMount;
        ISD::Mount *m_Mount {nullptr};
        static double RangePA(double pa);
        static double Range360(double r);

    signals:
        void   changedPierside(ISD::Mount::PierSide index);
};
