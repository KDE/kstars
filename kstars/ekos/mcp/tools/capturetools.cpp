/*
    SPDX-FileCopyrightText: 2026 Thomas Nemer <thomas.nemer@fortytwo.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "capturetools.h"

#include "../mcptoolregistry.h"
#include "ekos/manager.h"
#include "ekos/capture/capture.h"
#include "ekos/ekos.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>

namespace MCP::Tools
{

void initCaptureTools(MCP::ToolRegistry *registry, Ekos::Manager *manager)
{
    // capture_status — returns current capture module status
    registry->registerTool(
    {
        QStringLiteral("capture_status"),
        QStringLiteral("Returns the current capture module status, job counts, active camera, filter, "
                       "and target name. Reports the lead camera train only."),
        {},
        [manager](const QJsonObject &, QString & error) -> QJsonValue
        {
            auto *capture = manager->captureModule();
            if (!capture)
            {
                error = "Capture module not available";
                return {};
            }
            return QJsonObject {
                { QStringLiteral("status"),      Ekos::getCaptureStatusString(capture->status(), false) },
                { QStringLiteral("jobCount"),    capture->getJobCount() },
                { QStringLiteral("pendingJobs"), capture->getPendingJobCount() },
                { QStringLiteral("camera"),      capture->camera() },
                { QStringLiteral("filter"),      capture->filter() },
                { QStringLiteral("targetName"),  capture->getTargetName() }
            };
        }
    });

    // capture_get_jobs — returns details for every job in the sequence queue
    registry->registerTool(
    {
        QStringLiteral("capture_get_jobs"),
        QStringLiteral("Returns the list of sequence jobs with their state, filter, and image progress. "
                       "Reports the lead camera train only."),
        {},
        [manager](const QJsonObject &, QString & error) -> QJsonValue
        {
            auto *capture = manager->captureModule();
            if (!capture)
            {
                error = "Capture module not available";
                return {};
            }
            QJsonArray jobs;
            const int count = capture->getJobCount();
            for (int i = 0; i < count; ++i)
            {
                jobs.append(QJsonObject
                {
                    { QStringLiteral("index"),    i },
                    { QStringLiteral("state"),    capture->getJobState(i) },
                    { QStringLiteral("filter"),   capture->getJobFilterName(i) },
                    { QStringLiteral("progress"), capture->getJobImageProgress(i) },
                    { QStringLiteral("count"),    capture->getJobImageCount(i) }
                });
            }
            return QJsonObject { { QStringLiteral("jobs"), jobs } };
        }
    });

    // capture_start — starts or resumes the capture sequence
    registry->registerTool(
    {
        QStringLiteral("capture_start"),
        QStringLiteral("Starts the capture sequence. Also restarts an aborted sequence, "
                       "resuming at the remaining frame counts. Acts on the lead camera "
                       "train only."),
        {},
        [manager](const QJsonObject &, QString & error) -> QJsonValue
        {
            auto *capture = manager->captureModule();
            if (!capture)
            {
                error = "Capture module not available";
                return {};
            }
            // Capture::start() returns the name of the train it started on;
            // an empty name means the train could not be resolved. A start
            // with no pending job still reports success — capture_status
            // reports the resulting state.
            const QString train = capture->start();
            return QJsonObject {
                { QStringLiteral("success"), !train.isEmpty() },
                { QStringLiteral("train"),   train }
            };
        }
    });

    // capture_stop — stops the capture sequence (returns to idle)
    registry->registerTool(
    {
        QStringLiteral("capture_stop"),
        QStringLiteral("Stops the capture sequence and returns the module to idle. "
                       "Acts on the lead camera train only."),
        {},
        [manager](const QJsonObject &, QString & error) -> QJsonValue
        {
            auto *capture = manager->captureModule();
            if (!capture)
            {
                error = "Capture module not available";
                return {};
            }
            // Capture::stop() is Q_NOREPLY — success acknowledges dispatch
            // only; capture_status reports the resulting state.
            capture->stop();
            return QJsonObject { { QStringLiteral("success"), true } };
        }
    });

    // capture_abort — aborts the capture sequence immediately
    registry->registerTool(
    {
        QStringLiteral("capture_abort"),
        QStringLiteral("Aborts the capture sequence immediately. Acts on the lead camera "
                       "train only."),
        {},
        [manager](const QJsonObject &, QString & error) -> QJsonValue
        {
            auto *capture = manager->captureModule();
            if (!capture)
            {
                error = "Capture module not available";
                return {};
            }
            // Capture::abort() is Q_NOREPLY — success acknowledges dispatch
            // only; capture_status reports the resulting state.
            capture->abort();
            return QJsonObject { { QStringLiteral("success"), true } };
        }
    });

    // capture_suspend — suspends the capture sequence
    registry->registerTool(
    {
        QStringLiteral("capture_suspend"),
        QStringLiteral("Suspends the capture sequence immediately, aborting the current "
                       "exposure; guiding and other modules keep running. Resume with "
                       "capture_resume. Acts on the lead camera train only."),
        {},
        [manager](const QJsonObject &, QString & error) -> QJsonValue
        {
            auto *capture = manager->captureModule();
            if (!capture)
            {
                error = "Capture module not available";
                return {};
            }
            // Capture::suspend() is Q_NOREPLY — success acknowledges dispatch
            // only; capture_status reports the resulting state.
            capture->suspend();
            return QJsonObject { { QStringLiteral("success"), true } };
        }
    });

    // capture_pause — pauses after the current exposure completes
    registry->registerTool(
    {
        QStringLiteral("capture_pause"),
        QStringLiteral("Pauses the capture sequence after the current exposure completes; "
                       "guiding and other modules keep running. Resume with capture_resume. "
                       "Acts on the lead camera train only."),
        {},
        [manager](const QJsonObject &, QString & error) -> QJsonValue
        {
            auto *capture = manager->captureModule();
            if (!capture)
            {
                error = "Capture module not available";
                return {};
            }
            // Capture::pause() is Q_NOREPLY — success acknowledges dispatch
            // only; capture_status reports the resulting state
            // (Paused (Planned) until the exposure finishes, then Paused).
            capture->pause();
            return QJsonObject { { QStringLiteral("success"), true } };
        }
    });

    // capture_resume — resumes a suspended, paused, or aborted sequence
    registry->registerTool(
    {
        QStringLiteral("capture_resume"),
        QStringLiteral("Resumes a suspended, paused, or aborted capture sequence at the "
                       "remaining frame counts. Acts on the lead camera train only."),
        {},
        [manager](const QJsonObject &, QString & error) -> QJsonValue
        {
            auto *capture = manager->captureModule();
            if (!capture)
            {
                error = "Capture module not available";
                return {};
            }
            // Capture::start() returns the name of the train it started on;
            // an empty name means the train could not be resolved.
            switch (capture->status())
            {
                case Ekos::CAPTURE_SUSPENDED:
                {
                    // An externally suspended sequence leaves the active job
                    // JOB_BUSY, which start() cannot pick up (findNextPendingJob
                    // only selects idle or aborted jobs). Abort first to move
                    // the job to JOB_ABORTED — completed frame counts are
                    // preserved — then start() resumes the remaining frames.
                    capture->abort();
                    const QString train = capture->start();
                    return QJsonObject
                    {
                        { QStringLiteral("success"), !train.isEmpty() },
                        { QStringLiteral("train"),   train }
                    };
                }
                case Ekos::CAPTURE_PAUSED:
                case Ekos::CAPTURE_PAUSE_PLANNED:
                    // Resumes from the stored continue-action; also cancels a
                    // pause that has not taken effect yet.
                    capture->toggleSequence();
                    return QJsonObject { { QStringLiteral("success"), true } };
                case Ekos::CAPTURE_ABORTED:
                {
                    const QString train = capture->start();
                    return QJsonObject
                    {
                        { QStringLiteral("success"), !train.isEmpty() },
                        { QStringLiteral("train"),   train }
                    };
                }
                default:
                    error = QStringLiteral("Nothing to resume (current state: %1)")
                            .arg(Ekos::getCaptureStatusString(capture->status(), false));
                    return {};
            }
        }
    });

    // capture_load_sequence — loads an ESQ sequence file
    registry->registerTool(
    {
        QStringLiteral("capture_load_sequence"),
        QStringLiteral("Loads an Ekos Sequence Queue (.esq) file from the given path. "
                       "Acts on the lead camera train only."),
        {
            { QStringLiteral("path"), QStringLiteral("string"), QStringLiteral("Full path to the .esq sequence file to load."), true }
        },
        [manager](const QJsonObject & args, QString & error) -> QJsonValue
        {
            auto *capture = manager->captureModule();
            if (!capture)
            {
                error = "Capture module not available";
                return {};
            }
            const QString path = args[QStringLiteral("path")].toString();
            if (path.isEmpty())
            {
                error = "Parameter 'path' is required";
                return {};
            }
            const bool ok = capture->loadSequenceQueue(path);
            return QJsonObject { { QStringLiteral("success"), ok } };
        }
    });

    // capture_set_target — sets the target name for the sequence
    registry->registerTool(
    {
        QStringLiteral("capture_set_target"),
        QStringLiteral("Sets the target object name used when saving captured images. "
                       "Acts on the lead camera train only."),
        {
            { QStringLiteral("name"), QStringLiteral("string"), QStringLiteral("Target object name (e.g. 'M42')."), true }
        },
        [manager](const QJsonObject & args, QString & error) -> QJsonValue
        {
            auto *capture = manager->captureModule();
            if (!capture)
            {
                error = "Capture module not available";
                return {};
            }
            const QString name = args[QStringLiteral("name")].toString();
            if (name.isEmpty())
            {
                error = "Parameter 'name' is required";
                return {};
            }
            capture->setTargetName(name);
            return QJsonObject { { QStringLiteral("success"), true } };
        }
    });

    registry->classify(QStringLiteral("capture_status"),        /*ro*/true,  /*destr*/false, /*idemp*/true);
    registry->classify(QStringLiteral("capture_get_jobs"),      /*ro*/true,  /*destr*/false, /*idemp*/true);
    registry->classify(QStringLiteral("capture_start"),         /*ro*/false, /*destr*/false, /*idemp*/false);
    registry->classify(QStringLiteral("capture_stop"),          /*ro*/false, /*destr*/false, /*idemp*/true);
    registry->classify(QStringLiteral("capture_abort"),         /*ro*/false, /*destr*/false, /*idemp*/true);
    registry->classify(QStringLiteral("capture_suspend"),       /*ro*/false, /*destr*/false, /*idemp*/true);
    registry->classify(QStringLiteral("capture_pause"),         /*ro*/false, /*destr*/false, /*idemp*/true);
    registry->classify(QStringLiteral("capture_resume"),        /*ro*/false, /*destr*/false, /*idemp*/false);
    registry->classify(QStringLiteral("capture_load_sequence"), /*ro*/false, /*destr*/true,  /*idemp*/false);
    registry->classify(QStringLiteral("capture_set_target"),    /*ro*/false, /*destr*/false, /*idemp*/true);
}

} // namespace MCP::Tools
