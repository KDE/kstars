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
        /**
         * @brief setReversed Inform RotatorUtils whether the rotator is currently reversed.
         *        This is needed so that calcRotatorAngle() sends the correctly compensated
         *        raw angle to the driver: a reversed driver maps command A -> reports range360(-A),
         *        so to achieve a given camera PA we must send range360(offset - PA) instead of
         *        range360(PA - offset).
         */
        void   setReversed(bool reversed);

        /**
         * @brief parityReversed Whether the software-only direction correction (Options::RotatorParityReversed,
         *        exposed as the "Rotator direction reversed" checkbox in Align settings) is currently active.
         *        Independent of setReversed()/m_Reversed, which mirrors the driver's own ROTATOR_REVERSE switch.
         */
        bool   parityReversed() const
        {
            return m_ParityReversed;
        }
        /// Flip the in-memory parity flag as a trial correction (not persisted).
        void   trialToggleParity();
        /// Persist the current trial parity value (and update the Align settings checkbox) as confirmed-good.
        void   commitParity();
        /// Undo an unconfirmed trial, restoring the last persisted parity value.
        void   revertParity();

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
        bool   m_Reversed {false};
        // Software-only direction correction (global Options::RotatorParityReversed), independent
        // of m_Reversed (which mirrors the driver's own ROTATOR_REVERSE switch). See parityReversed().
        bool   m_ParityReversed {false};
        bool   m_PersistedParityReversed {false};
        /// Combined effective direction: driver-mirrored reversal XOR software parity correction.
        bool   effectiveReversed() const
        {
            return m_Reversed != m_ParityReversed;
        }
        ISD::Mount *m_Mount {nullptr};
        // initRotatorUtils() is called from multiple places (Capture's rotator panel, Align's
        // refreshOpticalTrain()) whenever a train is (re)activated. Track the pierSideChanged
        // connection so re-initializing doesn't stack duplicate connections onto the same mount.
        QMetaObject::Connection m_PierSideConnection;
        double m_StartAngle, m_EndAngle {0};
        double m_ShiftAngle, m_DiffAngle {0};
        QTime  m_StartTime, m_CurrentTime;
        int    m_DeltaTime = 0;
        double m_DeltaAngle = 0;
        int    m_TimeFrame = 0;
        bool   m_initParameter, m_CCW = true;

    Q_SIGNALS:
        void   changedPierside(ISD::Mount::PierSide index);
};
