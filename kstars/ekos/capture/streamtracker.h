/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "indi/indicamera.h"
#include "indi/indiguider.h"
#include "indi/indimount.h"

#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QPointF>

#include <functional>

namespace Ekos
{

/**
 * @class StreamTracker
 * @brief Keeps an extended target (planet, Moon, Sun) centered while a camera streams or records video.
 *
 * The camera driver measures the target in every frame and publishes the averaged offset from a lock
 * point in STREAM_TRACKING_OFFSET. StreamTracker only reads that number property, so it works when
 * KStars does not receive the video at all (e.g. StellarMate's streamer is the only video client).
 *
 * Tracking follows the driver's STREAM_TRACKING switch, whoever sets it: the stream window, the
 * StellarMate app or any other client. When the switch turns on, StreamTracker calibrates the guide
 * pulses against the measured offsets if needed, then sends correction pulses whenever the offset
 * leaves the deadband. It never runs while Ekos guiding is active on the same mount.
 */
class StreamTracker : public QObject
{
        Q_OBJECT

    public:
        typedef enum
        {
            TRACKER_IDLE,
            TRACKER_WAITING,
            TRACKER_CALIBRATING,
            TRACKER_TRACKING,
            TRACKER_LOST,
            TRACKER_PAUSED,
            TRACKER_ERROR
        } TrackerState;

        /// Untranslated state name for clients, e.g. "Calibrating"
        static QString stateName(TrackerState state);

        explicit StreamTracker(QObject *parent = nullptr);

        /**
         * @brief setDevices Camera that measures the target, and guider used to correct it.
         * @param guider device that accepts timed guide pulses (mount or ST4). Tracking cannot correct without one.
         */
        void setDevices(ISD::Camera *camera, ISD::Mount *mount, ISD::Guider *guider);

        /// Returns true while Ekos guiding is active; StreamTracker stays out of the way when it is.
        void setGuidingActiveCallback(std::function<bool()> callback)
        {
            m_IsGuidingActive = std::move(callback);
        }

        TrackerState state() const
        {
            return m_State;
        }

        /// Forget the calibration, e.g. after rotating the camera
        void clearCalibration();

    Q_SIGNALS:
        void newLog(const QString &text);
        void newState(TrackerState state);
        /// Correction sent, in milliseconds: positive RA is West, positive DEC is North
        void newCorrection(double offsetX, double offsetY, int raPulse, int decPulse);

    private:
        typedef enum
        {
            CAL_BASELINE,
            CAL_RA_OUT,
            CAL_RA_BACK,
            CAL_DEC_OUT,
            CAL_DEC_BACK
        } CalibrationStage;

        void processProperty(INDI::Property prop);
        void processReport(INDI::PropertyNumber offset);

        void start();
        void stop();
        void setState(TrackerState state);

        void processCalibration(const QPointF &position);
        void processTracking(const QPointF &offset);
        bool finishCalibration();
        void failCalibration(const QString &reason);

        // Positive raMs pulses West, positive decMs pulses North. Gates out the reports until the mount settles.
        void pulse(int raMs, int decMs);
        // True if a report can be used: received after the last pulse settled and after the window in progress
        bool reportUsable();

        int currentBinning() const;

        QPointer<ISD::Camera> m_Camera;
        QPointer<ISD::Mount> m_Mount;
        QPointer<ISD::Guider> m_Guider;
        std::function<bool()> m_IsGuidingActive;

        TrackerState m_State { TRACKER_IDLE };
        double m_LastSequence { -1 };
        int m_LostReports { 0 };

        // Settle gate after each pulse
        QElapsedTimer m_Clock;
        qint64 m_GateUntil { 0 };
        int m_SkipReports { 0 };

        // Calibration: pixels moved per millisecond of a West pulse and of a North pulse
        bool m_Calibrated { false };
        QPointF m_RAVector, m_DECVector;
        int m_CalibrationBinning { 1 };
        ISD::Mount::PierSide m_CalibrationPierSide { ISD::Mount::PIER_UNKNOWN };

        CalibrationStage m_Stage { CAL_BASELINE };
        QPointF m_Baseline;
        int m_StagePulseTotal { 0 };
        int m_RATotal { 0 }, m_DECTotal { 0 };
};

}
