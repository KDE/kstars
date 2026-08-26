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
#include <QVector>

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

        // Short exposure used ONLY for pulse_response phases, to resolve RA's dead-time
        // transient in the PID Auto-Tune fit (see pid_autotune.py's resolution_limited flag /
        // _estimate_dead_time_s()) -- confirmed 2026-08-26 that RA settles within a single
        // ~2s guide-cadence frame, so the normal exposure structurally can never sample the
        // transient it needs to fit dead time; every run gets resolution_limited=true and
        // confidence="low" regardless of data volume or retraining. Changing exposure on an
        // already-running stream isn't supported by the camera pipeline (Camera::
        // setStreamExposure() is only ever applied by Guide::startGuideStreaming() at
        // (re)start, see guide.cpp), so this requires a full stop+restart of guiding.
        // Measured on real hardware (ToupTek GPM462M, 3 trials): 3.34-3.46s per restart,
        // mean 3.38s, no errors. Done exactly twice per collection run -- once entering the
        // pulse_response block, once leaving it, both in STATE_PRECHECK below -- not once per
        // phase (there are 24 pulse_response phases; 48 restarts would still only cost ~3 min,
        // but there is no reason to pay it 24x over when paying it once covers the whole
        // block, and fewer stream transitions means fewer chances for something to go wrong
        // unattended overnight).
        static constexpr double PULSE_RESPONSE_EXPOSURE_S = 0.25;
        bool m_ExposureSwitchedForPulseResponse { false };
        double m_OrigGuideExposure { -1.0 };

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
            // True for a pulse fired immediately after another pulse of the same axis/
            // direction/magnitude (no reversal in between): the mount should already be
            // engaged in this direction, so any dead-time/backlash gap that shows up on
            // the preceding ("cold", reversal) pulse should not reappear here. Comparing
            // cold vs. warm dead-time is how a real mechanical backlash/windup signature
            // is told apart from e.g. camera/processing latency, which would affect both
            // equally. See pulse_response_fit.py's KAPPA_MAX comment for why the previous
            // paired-differencing approach couldn't distinguish these on its own.
            bool pulseWarm {false};
        };
        QList<ProtocolPhase> m_Phases;

        // Writes one captured segment as a session and flushes the log. recordedDuration is
        // what the segment was meant to run for: the trainer discards drift sessions much
        // shorter than it.
        void flushPhaseSegment(const ProtocolPhase &phase, int recordedDuration);

        // Rough per-phase duration estimate (seconds), used only to weight the progress bar
        // by expected time rather than raw phase count; see start()'s comment for why a
        // count-based progress bar is badly misleading for Harmonic Drive specifically.
        int estimatePhaseSeconds(const ProtocolPhase &phase, bool newPosition) const;
        // Sum of estimatePhaseSeconds() for every phase completed so far.
        int estimatedElapsedSeconds() const;
        QVector<int> m_PhaseEstimatedSeconds;
        int m_TotalEstimatedSeconds { 0 };

        ProtocolState m_State { STATE_IDLE };
        QTimer m_ProtocolTimer;
        double m_TargetAz { 0 };
        double m_TargetAlt { 0 };
        int m_SettlingTimer { 0 };
        // Wall-clock BACKSTOP for STATE_CAPTURING_DATA phases (a safety ceiling, not the
        // primary stop condition -- see m_TargetFrames and phaseTimedOut in
        // processProtocol()). Counts down at 1Hz regardless of actual frame arrival rate.
        int m_CaptureTimer { 0 };
        // Frame-count goal for the current STATE_CAPTURING_DATA phase, computed once at
        // phase start from that phase's nominal durationSeconds/exposure design point.
        // The phase actually completes when m_PhaseData reaches this count (or the
        // m_CaptureTimer backstop expires, whichever comes first) -- not on a fixed clock.
        // A fixed-duration-only design silently starves the sample count whenever real
        // per-frame cadence is slower than the exposure the duration was tuned for (e.g. a
        // Debug build's processing overhead nearly doubled real frame dt vs. a Release
        // build in one measured case, halving frames-per-phase and degrading the
        // downstream PE-period fit). Targeting frame count directly is cadence-agnostic.
        int m_TargetFrames { 0 };
        int m_AbortRetries { 0 };
        // Ticks to wait after calling m_Guide->guide() before trusting a still-IDLE/ABORTED
        // status as a real failure worth retrying -- see STATE_PULSE_RESPONSE_INIT.
        int m_GuideCallGraceTicks { 0 };
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