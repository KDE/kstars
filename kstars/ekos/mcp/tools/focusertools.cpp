/*
    SPDX-FileCopyrightText: 2026 Thomas Nemer <thomas.nemer@fortytwo.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "focusertools.h"
#include "devicelookup.h"
#include "../mcptoolregistry.h"

#include "indi/indifocuser.h"
#include "indi/indilistener.h"
#include "indi/indistd.h"

#include <basedevice.h>
#include <indiproperty.h>
#include <indipropertynumber.h>

#include <cstdlib>

#include <QJsonObject>
#include <QJsonValue>

namespace MCP::Tools
{

static ISD::Focuser *resolveFocuser(const QString &name, QString &error)
{
    if (!name.isEmpty())
    {
        auto dev = findDeviceByName(name, error);
        if (!dev) return nullptr;
        // ISD::Focuser is owned by GenericDevice via m_ConcreteDevices, not a
        // subclass of it — use the typed accessor instead of a dynamic_cast.
        auto *f = dev->getFocuser();
        if (!f)
        {
            error = QStringLiteral("Device '%1' is not a focuser").arg(name);
            return nullptr;
        }
        return f;
    }
    auto dev = findFirstDeviceByInterface(INDI::BaseDevice::FOCUSER_INTERFACE, error);
    if (!dev) return nullptr;
    return dev->getFocuser();
}

// Reads ABS_FOCUS_POSITION from the focuser device. Returns true and writes
// the current position into *pos on success.
static bool readAbsPosition(ISD::Focuser *focuser, int32_t *pos)
{
    INDI::Property prop = focuser->getProperty(QStringLiteral("ABS_FOCUS_POSITION"));
    if (!prop.isValid() || prop.getType() != INDI_NUMBER) return false;
    auto nvp = INDI::PropertyNumber(prop);
    if (nvp.count() < 1) return false;
    *pos = static_cast<int32_t>(nvp[0].getValue());
    return true;
}

void initFocuserTools(ToolRegistry *registry)
{
    // -----------------------------------------------------------------------
    // focuser_status
    // -----------------------------------------------------------------------
    registry->registerTool(
    {
        QStringLiteral("focuser_status"),
        QStringLiteral("Return focuser device state: name, current absolute position, max position, "
                       "and capability flags (canAbsMove, canRelMove, hasBacklash)."),
        {
            {
                QStringLiteral("name"), QStringLiteral("string"),
                QStringLiteral("Device name. Defaults to first connected focuser."), false
            }
        },
        [](const QJsonObject & args, QString & error) -> QJsonValue
        {
            auto focuser = resolveFocuser(args[QStringLiteral("name")].toString(), error);
            if (!focuser) return {};

            QJsonObject result;
            result[QStringLiteral("name")]        = focuser->getDeviceName();
            result[QStringLiteral("maxPosition")] = static_cast<int>(focuser->getmaxPosition());
            result[QStringLiteral("canAbsMove")]  = focuser->canAbsMove();
            result[QStringLiteral("canRelMove")]  = focuser->canRelMove();
            result[QStringLiteral("hasBacklash")] = focuser->hasBacklash();

            int32_t pos = 0;
            if (readAbsPosition(focuser, &pos))
                result[QStringLiteral("position")] = pos;

            return result;
        }
    });

    // -----------------------------------------------------------------------
    // focuser_move_absolute
    // -----------------------------------------------------------------------
    registry->registerTool(
    {
        QStringLiteral("focuser_move_absolute"),
        QStringLiteral("Move the focuser to an absolute step position. Validated against the driver's "
                       "max position. DO NOT call while Ekos autofocus is running."),
        {
            { QStringLiteral("name"),     QStringLiteral("string"),  QStringLiteral("Device name. Defaults to first connected focuser."), false },
            { QStringLiteral("position"), QStringLiteral("integer"), QStringLiteral("Target absolute position in driver steps."), true }
        },
        [](const QJsonObject & args, QString & error) -> QJsonValue
        {
            auto focuser = resolveFocuser(args[QStringLiteral("name")].toString(), error);
            if (!focuser) return {};
            if (!focuser->canAbsMove())
            {
                error = "Focuser does not support absolute moves";
                return {};
            }

            const int position = args[QStringLiteral("position")].toInt();
            if (position < 0)
            {
                error = "position must be >= 0";
                return {};
            }
            const uint32_t maxP = focuser->getmaxPosition();
            if (maxP > 0 && static_cast<uint32_t>(position) > maxP)
            {
                error = QStringLiteral("position %1 exceeds max %2").arg(position).arg(maxP);
                return {};
            }

            if (!focuser->moveAbs(position))
            {
                error = "Move request rejected by driver";
                return {};
            }
            return QJsonObject { { QStringLiteral("success"), true } };
        }
    });

    // -----------------------------------------------------------------------
    // focuser_move_relative
    // -----------------------------------------------------------------------
    registry->registerTool(
    {
        QStringLiteral("focuser_move_relative"),
        QStringLiteral("Move the focuser by a relative number of steps. Positive moves outward, negative moves inward "
                       "(driver-dependent; verify against your hardware). DO NOT call while Ekos autofocus is running."),
        {
            { QStringLiteral("name"),  QStringLiteral("string"),  QStringLiteral("Device name. Defaults to first connected focuser."), false },
            { QStringLiteral("steps"), QStringLiteral("integer"), QStringLiteral("Signed step count. Positive = outward, negative = inward."), true }
        },
        [](const QJsonObject & args, QString & error) -> QJsonValue
        {
            auto focuser = resolveFocuser(args[QStringLiteral("name")].toString(), error);
            if (!focuser) return {};
            if (!focuser->canRelMove())
            {
                error = "Focuser does not support relative moves";
                return {};
            }
            const int steps = args[QStringLiteral("steps")].toInt();
            if (steps == 0)
            {
                error = "steps must not be zero";
                return {};
            }
            // INDI's REL_FOCUS_POSITION is unsigned; direction is carried by the FOCUS_MOTION
            // switch. Set the direction first via focusIn/focusOut, then move by |steps|.
            if (steps < 0) focuser->focusIn();
            else           focuser->focusOut();
            if (!focuser->moveRel(std::abs(steps)))
            {
                error = "Move request rejected by driver";
                return {};
            }
            return QJsonObject { { QStringLiteral("success"), true } };
        }
    });

    // -----------------------------------------------------------------------
    // focuser_abort_move
    // -----------------------------------------------------------------------
    registry->registerTool(
    {
        QStringLiteral("focuser_abort_move"),
        QStringLiteral("Stop any in-flight focuser motion."),
        {
            {
                QStringLiteral("name"), QStringLiteral("string"),
                QStringLiteral("Device name. Defaults to first connected focuser."), false
            }
        },
        [](const QJsonObject & args, QString & error) -> QJsonValue
        {
            auto focuser = resolveFocuser(args[QStringLiteral("name")].toString(), error);
            if (!focuser) return {};
            focuser->stop();
            return QJsonObject { { QStringLiteral("success"), true } };
        }
    });

    registry->classify(QStringLiteral("focuser_status"),        /*ro*/true,  /*destr*/false, /*idemp*/true);
    registry->classify(QStringLiteral("focuser_move_absolute"), /*ro*/false, /*destr*/false, /*idemp*/true);
    registry->classify(QStringLiteral("focuser_move_relative"), /*ro*/false, /*destr*/false, /*idemp*/false);
    registry->classify(QStringLiteral("focuser_abort_move"),    /*ro*/false, /*destr*/false, /*idemp*/true);
}

} // namespace MCP::Tools
