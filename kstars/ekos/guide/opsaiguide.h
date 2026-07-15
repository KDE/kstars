/*
    SPDX-FileCopyrightText: 2026 Pavan <pk6122004@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_opsaiguide.h"
#include <QWizard>
#include <QTimer>
#include <QFile>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QElapsedTimer>

namespace Ekos
{

class OpsAIGuide : public QWizard, public Ui::OpsAIGuide
{
        Q_OBJECT

    public:
        explicit OpsAIGuide(QWidget *parent = nullptr);
        virtual ~OpsAIGuide() override = default;

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
            STATE_ERROR
        };
        Q_ENUM(ProtocolState)

        enum FrameErrorCode
        {
            FRAME_OK        = 0,
            FRAME_STAR_LOST = 1
        };

        // Exposed to EkosLive (ekoslive/message.cpp reads these by name via property()).
        Q_PROPERTY(QString mountType READ mountType)
        Q_PROPERTY(ProtocolState state READ state)
        Q_PROPERTY(int totalPhases READ totalPhases)
        Q_PROPERTY(int phasesRemaining READ phasesRemaining)

        QString mountType() const
        {
            return mountTypeCombo->currentText();
        }
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

    Q_SIGNALS:
        void aiGuideProgress(int current, int total, const QString &status);
        void aiGuideLog(const QString &message);
        void aiGuideComplete();
        // Emitted when the user clicks "Train in EkosLive"; carries the collected sysid data up to Guide.
        void trainInEkosLiveRequested(const QJsonObject &sysidData);

    protected:
        void initializePage(int id) override;
        // Ensure the guider is never left in Free-Drift or with enforced settings,
        // no matter how the wizard is dismissed (Finish, Cancel, window close, Esc).
        void done(int result) override;

    public slots:
        // Exposed to EkosLive / DBus so the training protocol can be started/stopped remotely.
        Q_INVOKABLE void slotStartProtocol();
        Q_INVOKABLE void slotStopProtocol();

        // Called by Guide with the EkosLive cloud training result to update the wizard UI.
        void onTrainingResult(bool success, const QJsonObject &result);

    private slots:
        void slotProcessProtocol();
        void slotOnGuideStats(double raErr, double decErr, int raPulse, int decPulse, double snr, double skyBg, int numStars);
        void slotExportOffline();
        void slotExportLogs();

    private:
        void appendLog(const QString &message);
        void enforceSettings();
        void restoreSettings();

        int m_TotalPhases { 0 };

        struct ProtocolPhase
        {
            double targetAlt;
            double azOffset; // -20.0 for East of Meridian (Pier West), +20.0 for West of Meridian (Pier East)
            int durationSeconds;
            bool freeDrift;
            // Pulse response fields (used only for HARMONIC_DRIVE)
            bool pulseResponse {false};
            QString pulseAxis;           // "RA" or "DEC"
            QString pulseDirection;      // "EAST", "WEST", "NORTH", "SOUTH"
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
        int m_AbortRetries { 0 };        ///< number of recovery attempts in current phase
        bool m_FreeDriftOverflow { false }; ///< set when free-drift accumulated pos exceeds safety limit

        QFile m_LogFile;
        QString m_LogFilename;
        QJsonObject m_SysIdData;
        QJsonArray m_PhaseData;

        QElapsedTimer m_FrameTimer;

        bool m_SettingsEnforced { false }; ///< true once originals are saved & standard mode forced
        int m_OrigRAAlgorithm { 0 };
        int m_OrigDECAlgorithm { 0 };
        bool m_OrigRAEnabled { true };
        bool m_OrigDECEnabled { true };
        bool m_OrigEastEnabled { true };
        bool m_OrigWestEnabled { true };
        bool m_OrigNorthEnabled { true };
        bool m_OrigSouthEnabled { true };
        double m_OrigMaxDeltaRMS { 2.0 };

        // Pulse response tracking (harmonic drive)
        int m_PulseFrameCount { 0 };
        int m_PulseSettleTimer { 0 };
        int m_PulseWatchdog { 0 };   ///< 1Hz backstop so pulse recording can't hang if the star is lost
        QJsonArray m_PulseResponseData;
};

}
