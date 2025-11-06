/*
    SPDX-FileCopyrightText: 2025 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "taskaction.h"
#include <QProcess>
#include <QTimer>

namespace Ekos
{

/**
 * @class ScriptAction
 * @brief Action to execute an external script or program
 *
 * ScriptAction runs an executable script and monitors its completion.
 * It captures stdout/stderr for logging and handles timeouts and failures
 * with configurable retry logic.
 */
class ScriptAction : public TaskAction
{
        Q_OBJECT

    public:
        explicit ScriptAction(QObject *parent = nullptr);
        ~ScriptAction() override;

        // Core execution interface
        bool start() override;
        bool abort() override;
        bool isAlreadyDone() override;

        // Configuration
        QString scriptPath() const
        {
            return m_scriptPath;
        }
        void setScriptPath(const QString &path)
        {
            m_scriptPath = path;
        }

        // Serialization
        QJsonObject toJson() const override;
        static ScriptAction* fromJson(const QJsonObject &json, QObject *parent = nullptr);

    private slots:
        void processFinished(int exitCode, QProcess::ExitStatus exitStatus);
        void processError(QProcess::ProcessError error);
        void readStandardOutput();
        void readStandardError();
        void handleTimeout();

    private:
        /**
         * @brief Validate that the script exists and is executable
         * @return true if script is valid
         */
        bool validateScript();

        QString m_scriptPath;
        QProcess *m_process = nullptr;
        QTimer *m_timeoutTimer = nullptr;
};

} // namespace Ekos
