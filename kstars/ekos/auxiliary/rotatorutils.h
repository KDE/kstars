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
        void   initTimeFrame(const double EndAngle);
        int    calcTimeFrame(const double CurrentAngle);
        void   startTimeFrame(const double StartAngle);

    private:
        RotatorUtils();
        ~RotatorUtils();
        static RotatorUtils *m_Instance;

        ISD::Mount::PierSide m_CalPierside {ISD::Mount::PIER_WEST};
        ISD::Mount::PierSide m_ImgPierside {ISD::Mount::PIER_UNKNOWN};
        double m_Offset {0};
        bool   m_flippedMount {false};
        ISD::Mount *m_Mount {nullptr};
        double m_StartAngle, m_EndAngle {0};
        double m_ShiftAngle, m_DiffAngle {0};
        QTime  m_StartTime, m_CurrentTime;
        int    m_DeltaTime = 0;
        double m_DeltaAngle = 0;
        int    m_TimeFrame = 0;
        bool   m_initParameter, m_CCW = true;

    signals:
        void   changedPierside(ISD::Mount::PierSide index);
};
