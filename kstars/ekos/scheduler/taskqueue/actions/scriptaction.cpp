/*
    SPDX-FileCopyrightText: 2025 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scriptaction.h"
#include <ekos_scheduler_debug.h>

#include <QFile>
#include <QFileInfo>
#include <QJsonObject>

#include "auxiliary/ksutils.h"

namespace Ekos
{

ScriptAction::ScriptAction(QObject *parent)
    : TaskAction(parent)
{
    m_type = SCRIPT;
    m_process = new QProcess(this);
    m_timeoutTimer = new QTimer(this);
    m_timeoutTimer->setSingleShot(true);

    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &ScriptAction::processFinished);
    connect(m_process, &QProcess::errorOccurred,
            this, &ScriptAction::processError);
    connect(m_process, &QProcess::readyReadStandardOutput,
            this, &ScriptAction::readStandardOutput);
    connect(m_process, &QProcess::readyReadStandardError,
            this, &ScriptAction::readStandardError);
    connect(m_timeoutTimer, &QTimer::timeout,
            this, &ScriptAction::handleTimeout);
}

ScriptAction::~ScriptAction()
{
    if (m_process && m_process->state() != QProcess::NotRunning)
    {
        m_process->terminate();
        if (!m_process->waitForFinished(3000))
            m_process->kill();
    }
}

bool ScriptAction::start()
{
    qCInfo(KSTARS_EKOS_SCHEDULER) << "SCRIPT action starting:" << m_scriptPath;

    // Validate script
    if (!validateScript())
    {
        if (incrementRetry())
        {
            qCInfo(KSTARS_EKOS_SCHEDULER) << "Retrying SCRIPT action" << m_currentRetry << "/" << m_retries;
            Q_EMIT progress(QString("Retry %1/%2").arg(m_currentRetry).arg(m_retries));
            return start();  // Retry
        }
        else
        {
            setStatus(FAILED);
            Q_EMIT failed(m_errorMessage);
            return false;
        }
    }

    setStatus(RUNNING);
    Q_EMIT started();
    Q_EMIT progress(QString("Executing script: %1").arg(m_scriptPath));

    // Start the script
    KSUtils::startProcess(*m_process, m_scriptPath);

    if (!m_process->waitForStarted(5000))
    {
        setErrorMessage(QString("Failed to start script: %1").arg(m_scriptPath));
        qCWarning(KSTARS_EKOS_SCHEDULER) << m_errorMessage;

        if (incrementRetry())
        {
            qCInfo(KSTARS_EKOS_SCHEDULER) << "Retrying SCRIPT action" << m_currentRetry << "/" << m_retries;
            Q_EMIT progress(QString("Failed to start, retry %1/%2").arg(m_currentRetry).arg(m_retries));
            return start();  // Retry
        }
        else
        {
            setStatus(FAILED);
            Q_EMIT failed(m_errorMessage);
            return false;
        }
    }

    // Start timeout timer
    m_timeoutTimer->start(m_timeout * 1000);

    qCInfo(KSTARS_EKOS_SCHEDULER) << "Script started successfully, waiting for completion...";
    return true;
}

bool ScriptAction::abort()
{
    if (m_timeoutTimer)
        m_timeoutTimer->stop();

    if (m_process && m_process->state() != QProcess::NotRunning)
    {
        qCInfo(KSTARS_EKOS_SCHEDULER) << "Terminating script process...";
        m_process->terminate();
        if (!m_process->waitForFinished(3000))
        {
            qCWarning(KSTARS_EKOS_SCHEDULER) << "Force killing script process...";
            m_process->kill();
        }
    }

    setStatus(ABORTED);
    return true;
}

bool ScriptAction::isAlreadyDone()
{
    // Scripts always need to be executed
    return false;
}

bool ScriptAction::validateScript()
{
    if (m_scriptPath.isEmpty())
    {
        setErrorMessage("Script path is empty");
        qCWarning(KSTARS_EKOS_SCHEDULER) << m_errorMessage;
        return false;
    }

    QFileInfo fileInfo(m_scriptPath);

    if (!fileInfo.exists())
    {
        setErrorMessage(QString("Script does not exist: %1").arg(m_scriptPath));
        qCWarning(KSTARS_EKOS_SCHEDULER) << m_errorMessage;
        return false;
    }

    if (!fileInfo.isFile())
    {
        setErrorMessage(QString("Script path is not a file: %1").arg(m_scriptPath));
        qCWarning(KSTARS_EKOS_SCHEDULER) << m_errorMessage;
        return false;
    }

    if (!fileInfo.isExecutable())
    {
        setErrorMessage(QString("Script is not executable: %1").arg(m_scriptPath));
        qCWarning(KSTARS_EKOS_SCHEDULER) << m_errorMessage;
        return false;
    }

    return true;
}

void ScriptAction::processFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    m_timeoutTimer->stop();

    // Flush any remaining buffered output
    if (!m_stdoutBuffer.isEmpty())
    {
        QString line = m_stdoutBuffer.trimmed();
        if (!line.isEmpty())
        {
            qCInfo(KSTARS_EKOS_SCHEDULER) << "Script stdout:" << line;
            Q_EMIT progress(line);
        }
        m_stdoutBuffer.clear();
    }

    if (!m_stderrBuffer.isEmpty())
    {
        QString line = m_stderrBuffer.trimmed();
        if (!line.isEmpty())
        {
            qCWarning(KSTARS_EKOS_SCHEDULER) << "Script stderr:" << line;
            Q_EMIT progress(QString("Error: %1").arg(line));
        }
        m_stderrBuffer.clear();
    }

    qCInfo(KSTARS_EKOS_SCHEDULER) << "Script finished with exit code:" << exitCode
                                  << "status:" << (exitStatus == QProcess::NormalExit ? "Normal" : "Crashed");

    if (exitStatus == QProcess::CrashExit)
    {
        setErrorMessage(QString("Script crashed: %1").arg(m_scriptPath));
        qCWarning(KSTARS_EKOS_SCHEDULER) << m_errorMessage;

        if (incrementRetry())
        {
            qCInfo(KSTARS_EKOS_SCHEDULER) << "Retrying SCRIPT action (crashed)" << m_currentRetry << "/" << m_retries;
            Q_EMIT progress(QString("Script crashed, retry %1/%2").arg(m_currentRetry).arg(m_retries));
            start();
        }
        else
        {
            setStatus(FAILED);
            Q_EMIT failed(m_errorMessage);
        }
        return;
    }

    if (exitCode == 0)
    {
        Q_EMIT progress("Script completed successfully");
        setStatus(COMPLETED);
        Q_EMIT completed();
    }
    else
    {
        setErrorMessage(QString("Script failed with exit code %1: %2").arg(exitCode).arg(m_scriptPath));
        qCWarning(KSTARS_EKOS_SCHEDULER) << m_errorMessage;

        if (incrementRetry())
        {
            qCInfo(KSTARS_EKOS_SCHEDULER) << "Retrying SCRIPT action (non-zero exit)" << m_currentRetry << "/" << m_retries;
            Q_EMIT progress(QString("Script failed (exit code %1), retry %2/%3")
                            .arg(exitCode).arg(m_currentRetry).arg(m_retries));
            start();
        }
        else
        {
            setStatus(FAILED);
            Q_EMIT failed(m_errorMessage);
        }
    }
}

void ScriptAction::processError(QProcess::ProcessError error)
{
    m_timeoutTimer->stop();

    QString errorString;
    switch (error)
    {
        case QProcess::FailedToStart:
            errorString = "Failed to start";
            break;
        case QProcess::Crashed:
            errorString = "Crashed";
            break;
        case QProcess::Timedout:
            errorString = "Timed out";
            break;
        case QProcess::WriteError:
            errorString = "Write error";
            break;
        case QProcess::ReadError:
            errorString = "Read error";
            break;
        case QProcess::UnknownError:
        default:
            errorString = "Unknown error";
            break;
    }

    setErrorMessage(QString("Script process error (%1): %2").arg(errorString).arg(m_scriptPath));
    qCWarning(KSTARS_EKOS_SCHEDULER) << m_errorMessage;

    // Don't retry on FailedToStart as it's already handled in start()
    if (error != QProcess::FailedToStart)
    {
        if (incrementRetry())
        {
            qCInfo(KSTARS_EKOS_SCHEDULER) << "Retrying SCRIPT action (process error)" << m_currentRetry << "/" << m_retries;
            Q_EMIT progress(QString("Process error (%1), retry %2/%3")
                            .arg(errorString).arg(m_currentRetry).arg(m_retries));
            start();
        }
        else
        {
            setStatus(FAILED);
            Q_EMIT failed(m_errorMessage);
        }
    }
}

void ScriptAction::readStandardOutput()
{
    // Append new data to buffer
    m_stdoutBuffer.append(QString::fromUtf8(m_process->readAllStandardOutput()));

    // Emit complete lines only
    while (m_stdoutBuffer.contains('\n'))
    {
        int newlinePos = m_stdoutBuffer.indexOf('\n');
        QString line = m_stdoutBuffer.left(newlinePos).trimmed();
        m_stdoutBuffer.remove(0, newlinePos + 1);

        if (!line.isEmpty())
        {
            qCInfo(KSTARS_EKOS_SCHEDULER) << "Script stdout:" << line;
            Q_EMIT progress(line);
        }
    }
}

void ScriptAction::readStandardError()
{
    // Append new data to buffer
    m_stderrBuffer.append(QString::fromUtf8(m_process->readAllStandardError()));

    // Emit complete lines only
    while (m_stderrBuffer.contains('\n'))
    {
        int newlinePos = m_stderrBuffer.indexOf('\n');
        QString line = m_stderrBuffer.left(newlinePos).trimmed();
        m_stderrBuffer.remove(0, newlinePos + 1);

        if (!line.isEmpty())
        {
            qCWarning(KSTARS_EKOS_SCHEDULER) << "Script stderr:" << line;
            Q_EMIT progress(QString("Error: %1").arg(line));
        }
    }
}

void ScriptAction::handleTimeout()
{
    qCWarning(KSTARS_EKOS_SCHEDULER) << "Script execution timed out after" << m_timeout << "seconds:" << m_scriptPath;

    if (m_process && m_process->state() != QProcess::NotRunning)
    {
        m_process->terminate();
        if (!m_process->waitForFinished(3000))
            m_process->kill();
    }

    setErrorMessage(QString("Script execution timed out after %1 seconds: %2").arg(m_timeout).arg(m_scriptPath));

    if (incrementRetry())
    {
        qCInfo(KSTARS_EKOS_SCHEDULER) << "Retrying SCRIPT action (timeout)" << m_currentRetry << "/" << m_retries;
        Q_EMIT progress(QString("Timeout, retry %1/%2").arg(m_currentRetry).arg(m_retries));
        start();
    }
    else
    {
        setStatus(FAILED);
        Q_EMIT failed(m_errorMessage);
    }
}

QJsonObject ScriptAction::toJson() const
{
    QJsonObject json = TaskAction::toJson();
    json["script_path"] = m_scriptPath;
    return json;
}

ScriptAction* ScriptAction::fromJson(const QJsonObject &json, QObject *parent)
{
    ScriptAction *action = new ScriptAction(parent);

    action->setScriptPath(json["script_path"].toString());
    action->setTimeout(json["timeout"].toInt(300));  // Default 5 minutes
    action->setRetries(json["retries"].toInt(2));
    action->setFailureAction(static_cast<FailureAction>(json["failure_action"].toInt(ABORT_QUEUE)));

    return action;
}

} // namespace Ekos
