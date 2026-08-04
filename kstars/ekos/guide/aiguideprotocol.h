/*
    SPDX-FileCopyrightText: 2026 Pavan <pk6122004@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "internalguide/assistant_stats.h"

#include <QObject>
#include <QTimer>
#include <QFile>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QElapsedTimer>
#include <QList>
#include <QString>

namespace Ekos
{

class Guide;

class AIGuideProtocol : public QObject
{
        Q_OBJECT

    public:
        explicit AIGuideProtocol(Guide *guide);
        virtual ~AIGuideProtocol() override = default;

        enum ProtocolState
        {
            STATE_IDLE,
            STATE_PRECHECK,
            STATE_HORIZON_SCAN,
            STATE_SLEWING,
            STATE_SETTLING,
            STATE_CAPTURING_DATA,
            STATE_DRIFT_RECENTER,
            STATE_PULSE_RESPONSE_INIT,
            STATE_PULSE_SENDING,
            STATE_PULSE_RECORDING,
            STATE_PULSE_SETTLING,
            STATE_DONE,
            STATE_TRAINING,
            STATE_TRAINING_DONE,
            STATE_ERROR
        };
        Q_ENUM(ProtocolState)

        enum FrameErrorCode
        {
            FRAME_OK        = 0,
            FRAME_STAR_LOST = 1
        };

        // Accessors for Q_PROPERTY
        ProtocolState state() const
        {
            return m_State;
        }
        int totalPhases() const
        {
            return m_TotalPhases;
        }
        int phasesRemaining() const
        {
            return m_Phases.size();
        }
        const QJsonObject &sysIdData() const
        {
            return m_SysIdData;
        }
        const QString &logFilename() const
        {
            return m_LogFilename;
        }

        /**
         * @brief Auto-detect the mount class from the connected mount's device name via
         * mount_types.json (MountGuiderFactory::detectMountType()).
         * @return "WORM_GEAR" / "HARMONIC_DRIVE" / "DIRECT_DRIVE" / "NOT_FOUND". Returns
         * "NOT_FOUND" if no mount is connected or the name has no entry in the lookup table.
         */
        QString detectMountType() const;

    Q_SIGNALS:
        void protocolLog(const QString &message);
        void protocolProgress(int current, int total, const QString &status);
        void protocolComplete();
        void protocolStopped();
        void trainingRequested(const QJsonObject &sysidData);
        void trainingProgress(const QString &message);
        void trainingComplete();
        void trainingError(const QString &error);

    public slots:
        Q_INVOKABLE void start(const QString &mountType);
        Q_INVOKABLE void stop();
        Q_INVOKABLE void requestTraining();
        Q_INVOKABLE void loadAndComplete(const QString &jsonFilePath);
        void onTrainingResult(bool success, const QString &message);

    private slots:
        void processProtocol();
        void onGuideStats(double raErr, double decErr, int raPulse, int decPulse,
                          double snr, double skyBg, int numStars);

    private:
        void enforceSettings();
        void restoreSettings();
        QJsonObject buildFingerprint() const;
        void refreshFingerprint();
        // Actually sends the armed pulse-response test pulse. Called from onGuideStats()
        // at the first clean frame boundary once STATE_PULSE_SENDING is armed — NOT from
        // processProtocol()'s 1Hz tick — so the pulse can never land mid-exposure,
        // regardless of whether guiding is streaming or single-capture. See
        // STATE_PULSE_SETTLING / STATE_PULSE_SENDING handling in processProtocol() and
        // onGuideStats() for the full rationale.
        void firePulseResponsePulse();

        // Computes a recommended base RA/DEC proportional (+ conservative integral)
        // guide gain from the pulse_response sessions collected so far (a live C++
        // equivalent of offline_trainer/pid_autotune.py's SIMC-style calculation) and,
        // if the data is usable, applies it via Options::setRA/dECProportionalGain()
        // (+ integral gain) before the rest of the protocol runs. Called once from
        // STATE_PRECHECK, right after the PID Auto-Tune pulses (if any) are exhausted
        // and before the first real phase -- see pid_autotune_plan.md §7 for why this
        // must happen before, not after. A no-op if Options::aIPIDAutoTune() was
        // off or the collected data isn't usable (too few fits, inconsistent signs,
        // no calibration on record); the previously-set gain is left untouched.
        void applyPIDAutoTuneGainLock();
        // One axis of the above; returns true if it computed and applied a gain.
        bool computeAndApplyAxisGain(const QString &axis, double msPerArcsec);
        bool m_GainLocked { false };

        Guide *m_Guide { nullptr };
        int m_TotalPhases { 0 };

        struct ProtocolPhase
        {
            double targetAlt;
            double azOffset;
            int durationSeconds;
            bool freeDrift;
            bool pulseResponse {false};
            QString pulseAxis;
            QString pulseDirection;
            int pulseMagnitudeMs {0};
            int responseFrames {15};
            int settleSeconds {30};
        };
        QList<ProtocolPhase> m_Phases;

        // Writes one captured segment as a session and flushes the log. recordedDuration is
        // what the segment was meant to run for: the trainer discards drift sessions much
        // shorter than it.
        void flushPhaseSegment(const ProtocolPhase &phase, int recordedDuration);

        ProtocolState m_State { STATE_IDLE };
        QTimer m_ProtocolTimer;
        double m_TargetAz { 0 };
        double m_TargetAlt { 0 };
        int m_SettlingTimer { 0 };
        int m_CaptureTimer { 0 };
        int m_AbortRetries { 0 };
        bool m_FreeDriftOverflow { false };
        bool m_PhaseAborted { false };
        int m_SegmentSeconds { 0 };      ///< seconds captured in the current drift segment
        int m_RecenterTimer { 0 };
        int m_RecenterAttempts { 0 };
        double m_LastRAErrArcsec { 0.0 };
        double m_LastDECErrArcsec { 0.0 };

        QFile m_LogFile;
        QString m_LogFilename;
        QJsonObject m_SysIdData;
        QJsonArray m_PhaseData;

        QElapsedTimer m_FrameTimer;

        bool m_SettingsEnforced { false };
        bool m_SettingsChangedWarned { false };
        int m_OrigRAAlgorithm { 0 };
        int m_OrigDECAlgorithm { 0 };
        bool m_OrigRAEnabled { true };
        bool m_OrigDECEnabled { true };
        bool m_OrigEastEnabled { true };
        bool m_OrigWestEnabled { true };
        bool m_OrigNorthEnabled { true };
        bool m_OrigSouthEnabled { true };
        double m_OrigMaxDeltaRMS { 2.0 };

        // Live noise-floor (seeing) measurement: PHD2-style HPF sigma of the RA error
        HighPassFilter m_NoiseHPF;
        AxisStats m_NoiseStats;
        int m_NoiseFrameCount { 0 };
        int m_BestDriftNoiseFrames { 0 };

        int m_PulseFrameCount { 0 };
        int m_PulseSettleTimer { 0 };
        int m_PulseWatchdog { 0 };
        QJsonArray m_PulseResponseData;
        QJsonArray m_PulseBaselineData;  ///< pre-pulse frames, the fit's reference level
        qint64 m_PulseSentAtMs { 0 };    ///< pulse send time, t=0 for response frames
};

}