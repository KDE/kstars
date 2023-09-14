/*
    SPDX-FileCopyrightText: 2022 Toni Schriber
    SPDX-License-Identifier: GPL-2.0-or-later
*/


#pragma once

#include "indi/indimount.h"

class RotatorUtils : public QObject
{
        Q_OBJECT

    public:
        static RotatorUtils *Instance();
        static void release();

        void   initRotatorUtils(const QString &train);
        void   setImageFlip(bool state);
        bool   checkImageFlip();
        double calcRotatorAngle(double PositionAngle);
        double calcCameraAngle(double RotatorAngle, bool flippedImage);
        double calcOffsetAngle(double RotatorAngle, double PositionAngle);
        void   updateOffset(double Angle);
        void   setImagePierside(ISD::Mount::PierSide ImgPierside);
        ISD::Mount::PierSide getMountPierside();
        double DiffPA(double diff);

    private:
        RotatorUtils();
        ~RotatorUtils();
        static RotatorUtils *m_Instance;

        ISD::Mount::PierSide m_CalPierside {ISD::Mount::PIER_WEST};
        ISD::Mount::PierSide m_ImgPierside {ISD::Mount::PIER_UNKNOWN};
        double m_Offset {0};
        bool m_flippedMount {false};
        ISD::Mount *m_Mount {nullptr};

    signals:
        void   changedPierside(ISD::Mount::PierSide index);
};
