/*
    SPDX-FileCopyrightText: 2026 Thomas Nemer <thomas.nemer@fortytwo.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "schedulertools.h"
#include "../mcptoolregistry.h"

#include "ekos/manager.h"
#include "ekos/scheduler/scheduler.h"
#include "ekos/scheduler/schedulerprocess.h"
#include "ekos/ekos.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>

namespace MCP
{
namespace Tools
{

void initSchedulerTools(ToolRegistry *registry, Ekos::Manager *manager)
{
    // scheduler_status — returns scheduler state, current job name, and profile
    registry->registerTool(
    {
        QStringLiteral("scheduler_status"),
        QStringLiteral("Returns the current scheduler state, the name of the currently executing job, and the active equipment profile."),
        {},
        [manager](const QJsonObject &, QString & error) -> QJsonValue
        {
            auto *scheduler = manager->schedulerModule();
            if (!scheduler)
            {
                error = "Scheduler module not available";
                return {};
            }
            auto process = scheduler->process();
            if (!process)
            {
                error = "Scheduler process not available";
                return {};
            }
            return QJsonObject {
                { QStringLiteral("status"),     Ekos::getSchedulerStatusString(process->status(), false) },
                { QStringLiteral("currentJob"), process->currentJobName() },
                { QStringLiteral("profile"),    process->profile() }
            };
        }
    });

    // scheduler_jobs — returns all scheduler jobs as a JSON array
    registry->registerTool(
    {
        QStringLiteral("scheduler_jobs"),
        QStringLiteral("Returns all scheduler jobs as a JSON array."),
        {},
        [manager](const QJsonObject &, QString & error) -> QJsonValue
        {
            auto *scheduler = manager->schedulerModule();
            if (!scheduler)
            {
                error = "Scheduler module not available";
                return {};
            }
            auto process = scheduler->process();
            if (!process)
            {
                error = "Scheduler process not available";
                return {};
            }
            QString jsonStr = process->jsonJobs();
            QJsonParseError parseError;
            QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8(), &parseError);
            if (parseError.error != QJsonParseError::NoError)
            {
                // Distinguish a malformed payload from a genuinely empty queue.
                error = QStringLiteral("Failed to parse scheduler job list: %1").arg(parseError.errorString());
                return {};
            }
            QJsonArray jobsArray;
            if (doc.isArray())
                jobsArray = doc.array();
            else if (doc.isObject())
                jobsArray.append(doc.object());
            return QJsonObject { { QStringLiteral("jobs"), jobsArray } };
        }
    });

    // scheduler_current_job — returns name and details of the currently executing job
    registry->registerTool(
    {
        QStringLiteral("scheduler_current_job"),
        QStringLiteral("Returns the name and full details of the currently executing scheduler job."),
        {},
        [manager](const QJsonObject &, QString & error) -> QJsonValue
        {
            auto *scheduler = manager->schedulerModule();
            if (!scheduler)
            {
                error = "Scheduler module not available";
                return {};
            }
            auto process = scheduler->process();
            if (!process)
            {
                error = "Scheduler process not available";
                return {};
            }
            QString name    = process->currentJobName();
            QString jsonStr = process->currentJobJson();
            QJsonParseError parseError;
            QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8(), &parseError);
            QJsonObject details;
            if (parseError.error == QJsonParseError::NoError && doc.isObject())
                details = doc.object();
            return QJsonObject {
                { QStringLiteral("name"),    name },
                { QStringLiteral("details"), details }
            };
        }
    });

    // scheduler_start — starts the scheduler
    registry->registerTool(
    {
        QStringLiteral("scheduler_start"),
        QStringLiteral("Starts the scheduler to begin executing queued observation jobs."),
        {},
        [manager](const QJsonObject &, QString & error) -> QJsonValue
        {
            auto *scheduler = manager->schedulerModule();
            if (!scheduler)
            {
                error = "Scheduler module not available";
                return {};
            }
            auto process = scheduler->process();
            if (!process)
            {
                error = "Scheduler process not available";
                return {};
            }
            // SchedulerProcess::start() is Q_NOREPLY — success here acknowledges
            // dispatch only; scheduler_status reports the resulting state.
            process->start();
            return QJsonObject { { QStringLiteral("success"), true } };
        }
    });

    // scheduler_stop — stops the scheduler
    registry->registerTool(
    {
        QStringLiteral("scheduler_stop"),
        QStringLiteral("Stops the scheduler, halting execution of the current and queued observation jobs."),
        {},
        [manager](const QJsonObject &, QString & error) -> QJsonValue
        {
            auto *scheduler = manager->schedulerModule();
            if (!scheduler)
            {
                error = "Scheduler module not available";
                return {};
            }
            auto process = scheduler->process();
            if (!process)
            {
                error = "Scheduler process not available";
                return {};
            }
            // SchedulerProcess::stop() is Q_NOREPLY — success here acknowledges
            // dispatch only; scheduler_status reports the resulting state.
            process->stop();
            return QJsonObject { { QStringLiteral("success"), true } };
        }
    });

    // scheduler_load — loads a scheduler file from disk
    registry->registerTool(
    {
        QStringLiteral("scheduler_load"),
        QStringLiteral("Loads a scheduler list file (.esl) from the specified path. "
                       "Replaces the entire current job queue with the jobs from the file."),
        {
            { QStringLiteral("path"), QStringLiteral("string"), QStringLiteral("Absolute path to the scheduler list file (.esl) to load."), true }
        },
        [manager](const QJsonObject & args, QString & error) -> QJsonValue
        {
            auto *scheduler = manager->schedulerModule();
            if (!scheduler)
            {
                error = "Scheduler module not available";
                return {};
            }
            auto process = scheduler->process();
            if (!process)
            {
                error = "Scheduler process not available";
                return {};
            }
            QString path = args[QStringLiteral("path")].toString();
            if (path.isEmpty())
            {
                error = "path must not be empty";
                return {};
            }
            const bool ok = process->loadScheduler(path);
            if (!ok)
            {
                error = QStringLiteral("Failed to load scheduler list: %1").arg(path);
                return {};
            }
            return QJsonObject { { QStringLiteral("success"), true } };
        }
    });

    registry->classify(QStringLiteral("scheduler_status"),      /*ro*/true,  /*destr*/false, /*idemp*/true);
    registry->classify(QStringLiteral("scheduler_jobs"),        /*ro*/true,  /*destr*/false, /*idemp*/true);
    registry->classify(QStringLiteral("scheduler_current_job"), /*ro*/true,  /*destr*/false, /*idemp*/true);
    registry->classify(QStringLiteral("scheduler_start"),       /*ro*/false, /*destr*/false, /*idemp*/false);
    registry->classify(QStringLiteral("scheduler_stop"),        /*ro*/false, /*destr*/false, /*idemp*/true);
    registry->classify(QStringLiteral("scheduler_load"),        /*ro*/false, /*destr*/true,  /*idemp*/false);
}

} // namespace Tools
} // namespace MCP
