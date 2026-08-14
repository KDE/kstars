/*
    SPDX-FileCopyrightText: 2026 Thomas Nemer <thomas.nemer@fortytwo.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "aligntools.h"
#include "../mcptoolregistry.h"

#include "ekos/manager.h"
#include "ekos/align/align.h"
#include "ekos/ekos.h"
#include "kstarsdata.h"
#include "skyobjects/skypoint.h"

#include <QJsonObject>
#include <QJsonValue>
#include <QList>

namespace MCP
{
namespace Tools
{

bool alignBusy(Ekos::AlignState state)
{
    return !(state == Ekos::ALIGN_IDLE   || state == Ekos::ALIGN_COMPLETE ||
             state == Ekos::ALIGN_FAILED || state == Ekos::ALIGN_ABORTED);
}

QJsonObject makeAlignResultPayload(const QList<double> &solution, const QList<double> &fov,
                                   const QList<double> &alignError, double accuracyArcsec)
{
    if (solution.size() < 3 || solution[1] <= Ekos::INVALID_VALUE)
    {
        QJsonObject result;
        result["available"] = false;
        return result;
    }

    // getSolutionResult() returns [orientation_deg, RA_deg_J2000, DEC_deg_J2000].
    // Convert RA to hours and precess J2000 → JNow so the primary ra/dec match
    // mount_coords / mount_sync / mount_goto (all JNow). Keep the J2000 values
    // available separately for callers that need them.
    const double raJ2000Hours = solution[1] / 15.0;
    const double decJ2000Deg  = solution[2];

    SkyPoint p;
    p.setRA0(raJ2000Hours);
    p.setDec0(decJ2000Deg);
    p.updateCoordsNow(KStarsData::Instance()->updateNum());

    QJsonObject result;
    result["orientation"] = solution[0];
    result["ra"]          = p.ra().Hours();
    result["dec"]         = p.dec().Degrees();
    result["ra_j2000"]    = raJ2000Hours;
    result["dec_j2000"]   = decJ2000Deg;
    if (fov.size() >= 3)
        result["pixscale"] = fov[2];
    result["available"]   = true;

    // Error / accuracy. m_TargetDiffTotal is the 1e6 sentinel before any solve;
    // guard against it (and any stale value) so we never leak a bogus residual.
    // A real angular separation maxes out well under 1e6 arcsec.
    const bool errorAvailable = alignError.size() >= 3 && alignError[0] < 1e6;
    result["error_available"] = errorAvailable;
    if (errorAvailable)
    {
        const double total = alignError[0];
        result["error_arcsec"]     = total;
        result["error_ra_arcsec"]  = alignError[1];
        result["error_dec_arcsec"] = alignError[2];
        result["accuracy_arcsec"]  = accuracyArcsec;
        result["within_accuracy"]  = (total <= accuracyArcsec);

        // Mirror the Align UI tri-state exactly (align_solution.cpp:364-369) so
        // the MCP verdict and the on-screen green/yellow/red never disagree.
        QString accuracyStatus;
        if (total <= accuracyArcsec)
            accuracyStatus = QStringLiteral("within");
        else if (total < 1.5 * accuracyArcsec)
            accuracyStatus = QStringLiteral("close");
        else
            accuracyStatus = QStringLiteral("out");
        result["accuracy_status"] = accuracyStatus;
    }
    return result;
}

void initAlignTools(ToolRegistry *registry, Ekos::Manager *manager)
{
    // align_status — returns current align state, camera, and FOV
    registry->registerTool(
    {
        "align_status",
        "Returns the current alignment module status, active camera, and field of view dimensions, "
        "plus busy (true while the module is mid-operation) and solver_iterations / max_iterations. "
        "After align_solve, poll align_status until busy is false, then read align_result. The status "
        "string 'Successful' is a transient per-solve state during the slew_to_target convergence loop, "
        "not a terminal state — do not stop on it. solver_iterations is informational only (it resets "
        "to 0 on success and may read 0 after a fast solve); gate polling on busy, not on the count.",
        {},
        [manager](const QJsonObject &, QString & error) -> QJsonValue
        {
            auto *align = manager->alignModule();
            if (!align)
            {
                error = "Align module not available";
                return {};
            }
            QList<double> fovData = align->fov();
            QJsonObject fovObj;
            if (fovData.size() >= 2)
            {
                fovObj["width"]  = fovData[0];
                fovObj["height"] = fovData[1];
            }
            QJsonObject result;
            result["status"]            = Ekos::getAlignStatusString(align->status(), false);
            result["camera"]            = align->camera();
            result["fov"]               = fovObj;
            result["busy"]              = alignBusy(align->status());
            result["solver_iterations"] = align->getSolverIterations();
            result["max_iterations"]    = align->getMaxSolverIterations();
            return result;
        }
    });

    // align_solve — captures a frame and runs plate-solving asynchronously
    registry->registerTool(
    {
        "align_solve",
        "Captures a frame and starts plate-solving asynchronously. After calling this, poll align_status "
        "until busy is false, then read align_result — within_accuracy / error_arcsec report whether it "
        "converged. The status string 'Successful' is a per-solve transient during the slew_to_target "
        "convergence loop, not terminal; do not stop on it. "
        "WARNING: the post-solve action (sync / slew / nothing) is determined by the current Align module "
        "setting (sticky from the UI or last align_set_solver_action call). Use align_set_solver_action "
        "first to make it explicit, otherwise a stale UI selection may trigger an unintended slew.",
        {},
        [manager](const QJsonObject &, QString & error) -> QJsonValue
        {
            auto *align = manager->alignModule();
            if (!align)
            {
                error = "Align module not available";
                return {};
            }
            align->captureAndSolve();
            return QJsonObject { { QStringLiteral("success"), true } };
        }
    });

    // align_get_solver_action — returns the current post-solve action
    registry->registerTool(
    {
        "align_get_solver_action",
        "Returns the current post-solve action: 'sync', 'slew_to_target', or 'nothing'.",
        {},
        [manager](const QJsonObject &, QString & error) -> QJsonValue
        {
            auto *align = manager->alignModule();
            if (!align)
            {
                error = "Align module not available";
                return {};
            }
            QString action;
            switch (align->currentGOTOMode())
            {
                case Ekos::Align::GOTO_SYNC:
                    action = QStringLiteral("sync");
                    break;
                case Ekos::Align::GOTO_SLEW:
                    action = QStringLiteral("slew_to_target");
                    break;
                case Ekos::Align::GOTO_NOTHING:
                    action = QStringLiteral("nothing");
                    break;
            }
            QJsonObject result;
            result["action"] = action;
            return result;
        }
    });

    // align_set_solver_action — sets the post-solve action
    registry->registerTool(
    {
        "align_set_solver_action",
        "Sets what the Align module does after a successful plate solve. "
        "'nothing' = leave mount alone, 'sync' = sync mount to solved coords, "
        "'slew_to_target' = slew to coords previously set via align_set_target_coords. "
        "Important: this is a sticky setting — always set it explicitly before calling align_solve.",
        {
            { "action", "string", "One of: 'nothing', 'sync', 'slew_to_target'.", true }
        },
        [manager](const QJsonObject & args, QString & error) -> QJsonValue
        {
            auto *align = manager->alignModule();
            if (!align)
            {
                error = "Align module not available";
                return {};
            }
            const QString action = args["action"].toString();
            int mode = -1;
            if (action == QLatin1String("sync"))                mode = Ekos::Align::GOTO_SYNC;
            else if (action == QLatin1String("slew_to_target")) mode = Ekos::Align::GOTO_SLEW;
            else if (action == QLatin1String("nothing"))        mode = Ekos::Align::GOTO_NOTHING;
            else
            {
                error = "action must be one of: 'nothing', 'sync', 'slew_to_target'";
                return {};
            }
            align->setSolverAction(mode);
            return QJsonObject { { QStringLiteral("success"), true } };
        }
    });

    // align_set_target_coords — sets the target coords used when action is 'slew_to_target'
    registry->registerTool(
    {
        "align_set_target_coords",
        "Sets the target coordinates for 'slew_to_target' action. RA in decimal hours (0-24, J2000), "
        "Dec in decimal degrees (-90 to +90, J2000). Only meaningful when align_set_solver_action is 'slew_to_target'.",
        {
            { "ra",  "number", "Right Ascension in decimal hours (J2000).", true },
            { "dec", "number", "Declination in decimal degrees (J2000).",   true }
        },
        [manager](const QJsonObject & args, QString & error) -> QJsonValue
        {
            auto *align = manager->alignModule();
            if (!align)
            {
                error = "Align module not available";
                return {};
            }
            if (!args.contains("ra") || !args.contains("dec"))
            {
                error = "ra and dec are required";
                return {};
            }
            const double ra  = args["ra"].toDouble();
            const double dec = args["dec"].toDouble();
            align->setTargetCoords(ra, dec);
            return QJsonObject { { QStringLiteral("success"), true } };
        }
    });

    // align_get_target_coords — returns the current target coords
    registry->registerTool(
    {
        "align_get_target_coords",
        "Returns the current 'slew_to_target' target coordinates: ra in decimal hours (J2000), "
        "dec in decimal degrees (J2000). Returns {\"available\": false} if no target has been "
        "set this session.",
        {},
        [manager](const QJsonObject &, QString & error) -> QJsonValue
        {
            auto *align = manager->alignModule();
            if (!align)
            {
                error = "Align module not available";
                return {};
            }
            // Align::getTargetCoords reads m_TargetCoord unconditionally; before any
            // target is set, that SkyPoint is default-constructed and dec0 reads out
            // of the valid [-90, +90] range. Treat that as "no target set" rather
            // than leaking the uninitialized value.
            QList<double> tc = align->getTargetCoords();
            if (tc.size() < 2 || tc[1] < -90.0 || tc[1] > 90.0)
                return QJsonObject { { QStringLiteral("available"), false } };

            return QJsonObject {
                { QStringLiteral("available"), true },
                { QStringLiteral("ra"),        tc[0] },
                { QStringLiteral("dec"),       tc[1] }
            };
        }
    });

    // align_set_solver_mode — picks the plate-solver backend
    registry->registerTool(
    {
        "align_set_solver_mode",
        "Selects the plate-solver backend: 'local' uses the in-process StellarSolver (default, "
        "what most rigs want), 'remote' delegates to an INDI astrometry parser on a connected device.",
        {
            { "mode", "string", "One of: 'local', 'remote'.", true }
        },
        [manager](const QJsonObject & args, QString & error) -> QJsonValue
        {
            auto *align = manager->alignModule();
            if (!align)
            {
                error = "Align module not available";
                return {};
            }
            const QString mode = args["mode"].toString();
            int v = -1;
            if (mode == QLatin1String("local"))       v = Ekos::Align::SOLVER_LOCAL;
            else if (mode == QLatin1String("remote")) v = Ekos::Align::SOLVER_REMOTE;
            else
            {
                error = "mode must be one of: 'local', 'remote'";
                return {};
            }
            align->setSolverMode(v);
            return QJsonObject { { QStringLiteral("success"), true } };
        }
    });

    // align_result — returns the last plate-solve solution
    registry->registerTool(
    {
        "align_result",
        "Returns the last plate-solve solution. Primary fields (ra, dec) are in JNow and match the mount's "
        "coordinate system — safe to feed to mount_sync directly. ra in decimal hours (0-24), dec in decimal "
        "degrees (-90 to +90). ra_j2000 / dec_j2000 are the original solver output (J2000) for callers that "
        "need them. orientation in degrees (position angle), pixscale in arcsec/pixel. "
        "When error_available is true, also reports the pointing error solved-vs-target: "
        "error_arcsec (total), error_ra_arcsec, error_dec_arcsec, the configured "
        "accuracy_arcsec threshold, within_accuracy (bool), and accuracy_status "
        "('within' | 'close' | 'out', matching the Align UI green/yellow/red). "
        "Returns {\"available\": false} if no solution is available yet.",
        {},
        [manager](const QJsonObject &, QString & error) -> QJsonValue
        {
            auto *align = manager->alignModule();
            if (!align)
            {
                error = "Align module not available";
                return {};
            }
            return makeAlignResultPayload(align->getSolutionResult(), align->fov(),
                                          align->getAlignErrorResult(), align->getAccuracyThreshold());
        }
    });

    // align_load_and_slew — loads a FITS file and slews to its coordinates
    registry->registerTool(
    {
        "align_load_and_slew",
        "Loads a FITS file, plate-solves it, and slews the mount to the solved coordinates.",
        {
            { "path", "string", "Absolute path to the FITS file to load and solve.", true }
        },
        [manager](const QJsonObject & args, QString & error) -> QJsonValue
        {
            auto *align = manager->alignModule();
            if (!align)
            {
                error = "Align module not available";
                return {};
            }
            QString path = args["path"].toString();
            if (path.isEmpty())
            {
                error = "path must not be empty";
                return {};
            }
            align->loadAndSlew(path);
            return QJsonObject{{"success", true}};
        }
    });

    // align_abort — aborts the current alignment operation
    registry->registerTool(
    {
        "align_abort",
        "Aborts the current alignment or plate-solving operation.",
        {},
        [manager](const QJsonObject &, QString & error) -> QJsonValue
        {
            auto *align = manager->alignModule();
            if (!align)
            {
                error = "Align module not available";
                return {};
            }
            align->abort();
            return QJsonObject { { QStringLiteral("success"), true } };
        }
    });

    registry->classify(QStringLiteral("align_status"),            /*ro*/true,  /*destr*/false, /*idemp*/true);
    registry->classify(QStringLiteral("align_solve"),             /*ro*/false, /*destr*/false, /*idemp*/false);
    registry->classify(QStringLiteral("align_result"),            /*ro*/true,  /*destr*/false, /*idemp*/true);
    registry->classify(QStringLiteral("align_load_and_slew"),     /*ro*/false, /*destr*/false, /*idemp*/false);
    registry->classify(QStringLiteral("align_abort"),             /*ro*/false, /*destr*/false, /*idemp*/true);
    registry->classify(QStringLiteral("align_get_solver_action"), /*ro*/true,  /*destr*/false, /*idemp*/true);
    registry->classify(QStringLiteral("align_set_solver_action"), /*ro*/false, /*destr*/false, /*idemp*/true);
    registry->classify(QStringLiteral("align_set_target_coords"), /*ro*/false, /*destr*/false, /*idemp*/true);
    registry->classify(QStringLiteral("align_get_target_coords"), /*ro*/true,  /*destr*/false, /*idemp*/true);
    registry->classify(QStringLiteral("align_set_solver_mode"),   /*ro*/false, /*destr*/false, /*idemp*/true);
}

} // namespace Tools
} // namespace MCP
