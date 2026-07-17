/*
    SPDX-FileCopyrightText: 2026 Pavan <pk6122004@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

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

        ProtocolState m_State { STATE_IDLE };
        QTimer m_ProtocolTimer;
        double m_TargetAz { 0 };
        double m_TargetAlt { 0 };
        int m_SettlingTimer { 0 };
        int m_CaptureTimer { 0 };
        int m_AbortRetries { 0 };
        bool m_FreeDriftOverflow { false };

        QFile m_LogFile;
        QString m_LogFilename;
        QJsonObject m_SysIdData;
        QJsonArray m_PhaseData;

        QElapsedTimer m_FrameTimer;

        bool m_SettingsEnforced { false };
        int m_OrigRAAlgorithm { 0 };
        int m_OrigDECAlgorithm { 0 };
        bool m_OrigRAEnabled { true };
        bool m_OrigDECEnabled { true };
        bool m_OrigEastEnabled { true };
        bool m_OrigWestEnabled { true };
        bool m_OrigNorthEnabled { true };
        bool m_OrigSouthEnabled { true };
        double m_OrigMaxDeltaRMS { 2.0 };

        int m_PulseFrameCount { 0 };
        int m_PulseSettleTimer { 0 };
        int m_PulseWatchdog { 0 };
        QJsonArray m_PulseResponseData;
};

}