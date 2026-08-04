/*
    SPDX-FileCopyrightText: 2026 Pavan <pk6122004@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_aiguidewizard.h"
#include <QWizard>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>

namespace Ekos
{
class AIGuideProtocol;

class AIGuideWizard : public QWizard, public Ui::AIGuideWizard
{
        Q_OBJECT

    public:
        explicit AIGuideWizard(AIGuideProtocol *protocol, QWidget *parent = nullptr);
        virtual ~AIGuideWizard() override = default;

        // Q_PROPERTY exposed to EkosLive for remote monitoring (read via property()).
        Q_PROPERTY(QString mountType READ mountType)
        Q_PROPERTY(int state READ state)
        Q_PROPERTY(int totalPhases READ totalPhases)
        Q_PROPERTY(int phasesRemaining READ phasesRemaining)

        QString mountType() const
        {
            return mountTypeCombo->currentText();
        }
        int state() const;
        int totalPhases() const;
        int phasesRemaining() const;
        const QJsonObject &sysIdData() const;

    Q_SIGNALS:
        void aiGuideProgress(int current, int total, const QString &status);
        void aiGuideLog(const QString &message);
        void aiGuideComplete();
        void trainInEkosLiveRequested(const QJsonObject &sysidData);

    protected:
        void initializePage(int id) override;
        bool validateCurrentPage() override;
        void done(int result) override;
        void showEvent(QShowEvent *event) override;

    public slots:
        Q_INVOKABLE void slotStartProtocol();
        Q_INVOKABLE void slotStopProtocol();

        void onTrainingResult(bool success, const QJsonObject &result);

    private slots:
        void slotExportOffline();
        void slotExportLogs();

    private:
        void appendLog(const QString &message);
        void updateLockedSettingsLabel();

        AIGuideProtocol *m_Protocol { nullptr };
        bool m_AutoNavigating { false };
        // Combo text we last auto-set (empty if detection never succeeded yet). Re-checked
        // every time the wizard is shown, but only applied if the combo still shows either
        // the untouched Designer default or exactly this value, so a manual override
        // survives reopening the dialog, while a failed/missing detection can "heal" itself
        // (e.g. after mount_types.json gains an entry) without requiring an app restart.
        QString m_LastAutoDetectedMountType;
};

}