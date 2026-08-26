/*
    SPDX-FileCopyrightText: 2026 Thomas Nemer <thomas.nemer@fortytwo.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "focustools.h"
#include "../mcptoolregistry.h"

#include "ekos/manager.h"
#include "ekos/focus/focusmodule.h"
#include "ekos/focus/focus.h"
#include "ekos/ekos.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>

namespace MCP
{
namespace Tools
{

void initFocusTools(ToolRegistry *registry, Ekos::Manager *manager)
{
    // focus_status — returns status, focuser, camera, filter for the main focuser
    registry->registerTool(
    {
        "focus_status",
        "Returns the current focus module status including focuser device, camera device, and active filter.",
        {},
        [manager](const QJsonObject &, QString & error) -> QJsonValue
        {
            auto *focusModule = manager->focusModule();
            if (!focusModule)
            {
                error = "Focus module not available";
                return {};
            }
            auto focuser = focusModule->mainFocuser();
            if (!focuser)
            {
                error = "No focuser available";
                return {};
            }
            QJsonObject result;
            result["status"]  = Ekos::getFocusStatusString(focuser->status(), false);
            result["focuser"] = focuser->focuser();
            result["camera"]  = focuser->camera();
            result["filter"]  = focuser->filter();
            // Current measured HFR (pixels). -1 if no frame has been measured yet.
            result["hfr"]     = focuser->getHFR();
            return result;
        }
    });

    // -----------------------------------------------------------------------
    // focus_hfr_samples — V-curve sample series for the active/last autofocus run
    // -----------------------------------------------------------------------
    registry->registerTool(
    {
        "focus_hfr_samples",
        "Returns the HFR sample series for the active or most-recent autofocus run. Each entry has "
        "position (focuser steps), hfr (pixels), weight (fit weight, higher = more confident), and "
        "outlier (true if excluded from the curve fit). Empty array if no autofocus has run yet.",
        {},
        [manager](const QJsonObject &, QString & error) -> QJsonValue
        {
            auto *focusModule = manager->focusModule();
            if (!focusModule)
            {
                error = "Focus module not available";
                return {};
            }
            auto focuser = focusModule->mainFocuser();
            if (!focuser)
            {
                error = "No focuser available";
                return {};
            }

            const auto &positions = focuser->getFocusPositions();
            const auto &values    = focuser->getFocusHFRValues();
            const auto &weights   = focuser->getFocusWeights();
            const auto &outliers  = focuser->getFocusOutliers();

            QJsonArray samples;
            const int n = positions.size();
            for (int i = 0; i < n; ++i)
            {
                QJsonObject pt;
                pt["position"] = positions[i];
                pt["hfr"]      = i < values.size()   ? values[i]    : 0.0;
                pt["weight"]   = i < weights.size()  ? weights[i]   : 0.0;
                pt["outlier"]  = i < outliers.size() ? outliers[i]  : false;
                samples.append(pt);
            }
            return QJsonObject { { "samples", samples } };
        }
    });

    // focus_auto — starts autofocus on the main focuser
    registry->registerTool(
    {
        "focus_auto",
        "Starts the autofocus procedure on the main focuser.",
        {},
        [manager](const QJsonObject &, QString & error) -> QJsonValue
        {
            auto *focusModule = manager->focusModule();
            if (!focusModule)
            {
                error = "Focus module not available";
                return {};
            }
            auto focuser = focusModule->mainFocuser();
            if (!focuser)
            {
                error = "No focuser available";
                return {};
            }
            focuser->start();
            return QJsonObject { { "success", true } };
        }
    });

    // focus_abort — aborts autofocus
    registry->registerTool(
    {
        "focus_abort",
        "Aborts the current autofocus procedure.",
        {},
        [manager](const QJsonObject &, QString & error) -> QJsonValue
        {
            auto *focusModule = manager->focusModule();
            if (!focusModule)
            {
                error = "Focus module not available";
                return {};
            }
            auto focuser = focusModule->mainFocuser();
            if (!focuser)
            {
                error = "No focuser available";
                return {};
            }
            focuser->abort();
            return QJsonObject { { "success", true } };
        }
    });

    // focus_step_in — move focuser inward by the given number of steps/ticks
    registry->registerTool(
    {
        "focus_step_in",
        "Moves the focuser inward by the specified number of steps (ticks for absolute focusers, milliseconds for relative focusers). "
        "The focus_* family drives the autofocus algorithm; for direct focuser device moves see the focuser_* tools.",
        {
            { "steps", "integer", "Number of steps (or milliseconds) to move inward.", true }
        },
        [manager](const QJsonObject & args, QString & error) -> QJsonValue
        {
            auto *focusModule = manager->focusModule();
            if (!focusModule)
            {
                error = "Focus module not available";
                return {};
            }
            auto focuser = focusModule->mainFocuser();
            if (!focuser)
            {
                error = "No focuser available";
                return {};
            }
            int steps = args["steps"].toInt(0);
            if (steps <= 0)
            {
                error = "steps must be a positive integer";
                return {};
            }
            return QJsonObject { { "success", focuser->focusIn(steps) } };
        }
    });

    // focus_step_out — move focuser outward by the given number of steps/ticks
    registry->registerTool(
    {
        "focus_step_out",
        "Moves the focuser outward by the specified number of steps (ticks for absolute focusers, milliseconds for relative focusers). "
        "The focus_* family drives the autofocus algorithm; for direct focuser device moves see the focuser_* tools.",
        {
            { "steps", "integer", "Number of steps (or milliseconds) to move outward.", true }
        },
        [manager](const QJsonObject & args, QString & error) -> QJsonValue
        {
            auto *focusModule = manager->focusModule();
            if (!focusModule)
            {
                error = "Focus module not available";
                return {};
            }
            auto focuser = focusModule->mainFocuser();
            if (!focuser)
            {
                error = "No focuser available";
                return {};
            }
            int steps = args["steps"].toInt(0);
            if (steps <= 0)
            {
                error = "steps must be a positive integer";
                return {};
            }
            return QJsonObject { { "success", focuser->focusOut(steps) } };
        }
    });

    // focus_check — triggers a focus check against an optional HFR threshold
    registry->registerTool(
    {
        "focus_check",
        "Checks focus quality and triggers autofocus if the current HFR exceeds the threshold. "
        "Omit threshold or pass 0.0 to always trigger autofocus. The check is asynchronous; poll "
        "focus_status for the outcome. For a passive read of the last measured HFR, use focus_status.",
        {
            { "threshold", "number", "HFR threshold above which autofocus is triggered. Defaults to 0.0 (always trigger).", false }
        },
        [manager](const QJsonObject & args, QString & error) -> QJsonValue
        {
            auto *focusModule = manager->focusModule();
            if (!focusModule)
            {
                error = "Focus module not available";
                return {};
            }
            auto focuser = focusModule->mainFocuser();
            if (!focuser)
            {
                error = "No focuser available";
                return {};
            }
            const double threshold = args.value("threshold").toDouble(0.0);
            // checkFocus() is fire-and-forget (Q_NOREPLY void): success only
            // acknowledges dispatch; the real outcome is visible via focus_status.
            focuser->checkFocus(threshold);
            return QJsonObject { { "success", true } };
        }
    });

    registry->classify(QStringLiteral("focus_status"),       /*ro*/true,  /*destr*/false, /*idemp*/true);
    registry->classify(QStringLiteral("focus_auto"),         /*ro*/false, /*destr*/false, /*idemp*/false);
    registry->classify(QStringLiteral("focus_abort"),        /*ro*/false, /*destr*/false, /*idemp*/true);
    registry->classify(QStringLiteral("focus_step_in"),      /*ro*/false, /*destr*/false, /*idemp*/false);
    registry->classify(QStringLiteral("focus_step_out"),     /*ro*/false, /*destr*/false, /*idemp*/false);
    registry->classify(QStringLiteral("focus_check"),        /*ro*/false, /*destr*/false, /*idemp*/false);
    registry->classify(QStringLiteral("focus_hfr_samples"),  /*ro*/true,  /*destr*/false, /*idemp*/true);
}

} // namespace Tools
} // namespace MCP
