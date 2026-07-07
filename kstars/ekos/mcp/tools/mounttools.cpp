/*
    SPDX-FileCopyrightText: 2026 Thomas Nemer <thomas.nemer@fortytwo.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "mounttools.h"

#include "../mcptoolregistry.h"
#include "ekos/manager.h"
#include "ekos/mount/mount.h"
#include "indi/indimount.h"
#include "indi/indistd.h"
#include "kstarsdata.h"
#include "skycomponents/skymapcomposite.h"
#include "skyobjects/skyobject.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>

namespace MCP::Tools
{

// ---------------------------------------------------------------------------
// Helper: ISD::Mount::Status  →  string
// ---------------------------------------------------------------------------
static QString mountStatusString(ISD::Mount::Status s)
{
    switch (s)
    {
        case ISD::Mount::MOUNT_IDLE:
            return QStringLiteral("Idle");
        case ISD::Mount::MOUNT_MOVING:
            return QStringLiteral("Moving");
        case ISD::Mount::MOUNT_SLEWING:
            return QStringLiteral("Slewing");
        case ISD::Mount::MOUNT_TRACKING:
            return QStringLiteral("Tracking");
        case ISD::Mount::MOUNT_PARKING:
            return QStringLiteral("Parking");
        case ISD::Mount::MOUNT_PARKED:
            return QStringLiteral("Parked");
        case ISD::Mount::MOUNT_ERROR:
            return QStringLiteral("Error");
    }
    return QStringLiteral("Unknown");
}

// ---------------------------------------------------------------------------
// Helper: ISD::ParkStatus  →  string
// ---------------------------------------------------------------------------
static QString parkStatusString(ISD::ParkStatus s)
{
    switch (s)
    {
        case ISD::PARK_UNKNOWN:
            return QStringLiteral("Unknown");
        case ISD::PARK_PARKED:
            return QStringLiteral("Parked");
        case ISD::PARK_PARKING:
            return QStringLiteral("Parking");
        case ISD::PARK_UNPARKING:
            return QStringLiteral("Unparking");
        case ISD::PARK_UNPARKED:
            return QStringLiteral("Unparked");
        case ISD::PARK_ERROR:
            return QStringLiteral("Error");
    }
    return QStringLiteral("Unknown");
}

// ---------------------------------------------------------------------------
// Helper: ISD::Mount::PierSide  →  string
// ---------------------------------------------------------------------------
static QString pierSideString(ISD::Mount::PierSide s)
{
    switch (s)
    {
        case ISD::Mount::PIER_WEST:
            return QStringLiteral("West");
        case ISD::Mount::PIER_EAST:
            return QStringLiteral("East");
        case ISD::Mount::PIER_UNKNOWN:
            return QStringLiteral("Unknown");
    }
    return QStringLiteral("Unknown");
}

// ---------------------------------------------------------------------------
void initMountTools(MCP::ToolRegistry *registry, Ekos::Manager *manager)
{
    // -----------------------------------------------------------------------
    // mount_coords
    // -----------------------------------------------------------------------
    registry->registerTool(
    {
        QStringLiteral("mount_coords"),
        QStringLiteral("Return the current mount coordinates (RA/Dec, Alt/Az, hour angle, pier side, tracking status)."),
        {},
        [manager](const QJsonObject &, QString & error) -> QJsonValue
        {
            auto *mount = manager->mountModule();
            if (!mount)
            {
                error = "Mount module not available";
                return {};
            }

            // equatorialCoords() returns [RA in hours, Dec in degrees]
            const QList<double> eq = mount->equatorialCoords();
            // horizontalCoords() returns [Az in degrees, Alt in degrees]
            const QList<double> hz = mount->horizontalCoords();

            const double ra  = eq.size() >= 2 ? eq[0] : 0.0;
            const double dec = eq.size() >= 2 ? eq[1] : 0.0;
            const double az  = hz.size() >= 2 ? hz[0] : 0.0;
            const double alt = hz.size() >= 2 ? hz[1] : 0.0;

            return QJsonObject {
                { QStringLiteral("ra"),         ra },
                { QStringLiteral("dec"),        dec },
                { QStringLiteral("alt"),        alt },
                { QStringLiteral("az"),         az },
                { QStringLiteral("ha"),         mount->hourAngle() },
                { QStringLiteral("pierSide"),   pierSideString(mount->pierSide()) },
                { QStringLiteral("status"),     mountStatusString(mount->status()) },
                { QStringLiteral("parkStatus"), parkStatusString(mount->parkStatus()) }
            };
        }
    });

    // -----------------------------------------------------------------------
    // mount_goto
    // -----------------------------------------------------------------------
    registry->registerTool(
    {
        QStringLiteral("mount_goto"),
        QStringLiteral("Slew the mount to the given equatorial coordinates."),
        {
            { QStringLiteral("ra"),  QStringLiteral("number"), QStringLiteral("Right Ascension in hours (0–24)"),    true },
            { QStringLiteral("dec"), QStringLiteral("number"), QStringLiteral("Declination in degrees (-90 to +90)"), true }
        },
        [manager](const QJsonObject & args, QString & error) -> QJsonValue
        {
            auto *mount = manager->mountModule();
            if (!mount)
            {
                error = "Mount module not available";
                return {};
            }

            const double ra  = args[QStringLiteral("ra")].toDouble();
            const double dec = args[QStringLiteral("dec")].toDouble();
            const bool ok    = mount->slew(ra, dec);

            return QJsonObject { { QStringLiteral("success"), ok } };
        }
    });

    // -----------------------------------------------------------------------
    // mount_goto_target
    // -----------------------------------------------------------------------
    registry->registerTool(
    {
        QStringLiteral("mount_goto_target"),
        QStringLiteral("Slew the mount to a named sky object (resolved by KStars)."),
        {
            { QStringLiteral("name"), QStringLiteral("string"), QStringLiteral("Object name in canonical KStars form, e.g. \"M 42\" (with the space) or \"Vega\""), true }
        },
        [manager](const QJsonObject & args, QString & error) -> QJsonValue
        {
            auto *mount = manager->mountModule();
            if (!mount)
            {
                error = "Mount module not available";
                return {};
            }

            const QString name = args[QStringLiteral("name")].toString();

            // exact=true is required: with exact=false, CatalogsComponent's SQL
            // substring-matches the query against catalog long_names (`name LIKE
            // '%X%' OR long_name LIKE '%X%'`) and is searched before StarComponent.
            // "Polaris" matches NGC 2573 ("Polarissima Australis", a galaxy at the
            // SCP) and that hit wins. Callers must supply the canonical KStars
            // name ("M 42", not "M42") — use the catalog_search tool to resolve
            // a fuzzy query into a canonical name first.
            auto *object = KStarsData::Instance()->skyComposite()->findByName(name, true);
            if (!object)
            {
                error = QStringLiteral("Could not resolve target: %1").arg(args[QStringLiteral("name")].toString());
                return QJsonObject { { QStringLiteral("success"), false } };
            }
            object->updateCoordsNow(KStarsData::Instance()->updateNum());
            const bool ok = mount->slew(object->ra().Hours(), object->dec().Degrees());
            return QJsonObject { { QStringLiteral("success"), ok } };
        }
    });

    // -----------------------------------------------------------------------
    // mount_sync
    // -----------------------------------------------------------------------
    registry->registerTool(
    {
        QStringLiteral("mount_sync"),
        QStringLiteral("Sync the mount pointing model to the given coordinates."),
        {
            { QStringLiteral("ra"),  QStringLiteral("number"), QStringLiteral("Right Ascension in hours (0–24)"),    true },
            { QStringLiteral("dec"), QStringLiteral("number"), QStringLiteral("Declination in degrees (-90 to +90)"), true }
        },
        [manager](const QJsonObject & args, QString & error) -> QJsonValue
        {
            auto *mount = manager->mountModule();
            if (!mount)
            {
                error = "Mount module not available";
                return {};
            }

            const double ra  = args[QStringLiteral("ra")].toDouble();
            const double dec = args[QStringLiteral("dec")].toDouble();
            const bool ok    = mount->sync(ra, dec);

            return QJsonObject { { QStringLiteral("success"), ok } };
        }
    });

    // -----------------------------------------------------------------------
    // mount_park
    // -----------------------------------------------------------------------
    registry->registerTool(
    {
        QStringLiteral("mount_park"),
        QStringLiteral("Park the mount at its designated park position."),
        {},
        [manager](const QJsonObject &, QString & error) -> QJsonValue
        {
            auto *mount = manager->mountModule();
            if (!mount)
            {
                error = "Mount module not available";
                return {};
            }

            const bool ok = mount->park();
            return QJsonObject { { QStringLiteral("success"), ok } };
        }
    });

    // -----------------------------------------------------------------------
    // mount_unpark
    // -----------------------------------------------------------------------
    registry->registerTool(
    {
        QStringLiteral("mount_unpark"),
        QStringLiteral("Unpark the mount so it is ready for operation."),
        {},
        [manager](const QJsonObject &, QString & error) -> QJsonValue
        {
            auto *mount = manager->mountModule();
            if (!mount)
            {
                error = "Mount module not available";
                return {};
            }

            const bool ok = mount->unpark();
            return QJsonObject { { QStringLiteral("success"), ok } };
        }
    });

    // -----------------------------------------------------------------------
    // mount_abort
    // -----------------------------------------------------------------------
    registry->registerTool(
    {
        QStringLiteral("mount_abort"),
        QStringLiteral("Immediately stop all mount motion. Returns success once the abort "
                       "command is sent; the mount's status update is asynchronous, so "
                       "mount_coords may still report \"Slewing\" for up to ~1s. Wait "
                       "briefly before polling to confirm the stop."),
        {},
        [manager](const QJsonObject &, QString & error) -> QJsonValue
        {
            auto *mount = manager->mountModule();
            if (!mount)
            {
                error = "Mount module not available";
                return {};
            }

            const bool ok = mount->abort();
            return QJsonObject { { QStringLiteral("success"), ok } };
        }
    });

    // -----------------------------------------------------------------------
    // mount_set_meridian_flip
    // -----------------------------------------------------------------------
    registry->registerTool(
    {
        QStringLiteral("mount_set_meridian_flip"),
        QStringLiteral("Enable or disable the automatic meridian flip and set the flip hour-angle offset."),
        {
            { QStringLiteral("enabled"), QStringLiteral("boolean"), QStringLiteral("True to enable meridian flip, false to disable"), true },
            { QStringLiteral("hours"),   QStringLiteral("number"),  QStringLiteral("Hour-angle past meridian at which to flip (hours)"),  true }
        },
        [manager](const QJsonObject & args, QString & error) -> QJsonValue
        {
            auto *mount = manager->mountModule();
            if (!mount)
            {
                error = "Mount module not available";
                return {};
            }

            const bool   enabled = args[QStringLiteral("enabled")].toBool();
            const double hours   = args[QStringLiteral("hours")].toDouble();
            mount->setMeridianFlipValues(enabled, hours);

            return QJsonObject { { QStringLiteral("success"), true } };
        }
    });

    // -----------------------------------------------------------------------
    // mount_set_tracking
    // -----------------------------------------------------------------------
    registry->registerTool(
    {
        QStringLiteral("mount_set_tracking"),
        QStringLiteral("Enable or disable mount sidereal tracking."),
        {
            { QStringLiteral("enabled"), QStringLiteral("boolean"), QStringLiteral("True to enable tracking, false to disable."), true }
        },
        [manager](const QJsonObject & args, QString & error) -> QJsonValue
        {
            auto *mount = manager->mountModule();
            if (!mount)
            {
                error = "Mount module not available";
                return {};
            }
            mount->setTrackEnabled(args[QStringLiteral("enabled")].toBool());
            return QJsonObject { { QStringLiteral("success"), true } };
        }
    });

    // -----------------------------------------------------------------------
    // mount_set_track_mode
    // -----------------------------------------------------------------------
    registry->registerTool(
    {
        QStringLiteral("mount_set_track_mode"),
        QStringLiteral("Set the mount tracking rate. One of 'sidereal', 'lunar', 'solar', or 'custom'."),
        {
            {
                QStringLiteral("mode"), QStringLiteral("string"),
                QStringLiteral("'sidereal' | 'lunar' | 'solar' | 'custom'"), true
            }
        },
        [manager](const QJsonObject & args, QString & error) -> QJsonValue
        {
            auto *mount = manager->mountModule();
            if (!mount || !mount->activeMount())
            {
                error = "Mount not available";
                return {};
            }
            const QString mode = args[QStringLiteral("mode")].toString();
            int idx = -1;
            if      (mode == QLatin1String("sidereal")) idx = ISD::Mount::TRACK_SIDEREAL;
            else if (mode == QLatin1String("lunar"))    idx = ISD::Mount::TRACK_LUNAR;
            else if (mode == QLatin1String("solar"))    idx = ISD::Mount::TRACK_SOLAR;
            else if (mode == QLatin1String("custom"))   idx = ISD::Mount::TRACK_CUSTOM;
            else
            {
                error = "mode must be one of: sidereal, lunar, solar, custom";
                return {};
            }

            if (!mount->activeMount()->setTrackMode(static_cast<uint8_t>(idx)))
            {
                error = "Driver rejected setTrackMode";
                return {};
            }
            return QJsonObject { { QStringLiteral("success"), true } };
        }
    });

    // -----------------------------------------------------------------------
    // mount_set_slew_rate
    // -----------------------------------------------------------------------
    registry->registerTool(
    {
        QStringLiteral("mount_set_slew_rate"),
        QStringLiteral("Set the mount slew rate by 0-based index into the driver's supported rates "
                       "(see mount_get_slew_rates for the list)."),
        {
            {
                QStringLiteral("index"), QStringLiteral("integer"),
                QStringLiteral("0-based index into the driver's slew-rate list."), true
            }
        },
        [manager](const QJsonObject & args, QString & error) -> QJsonValue
        {
            auto *mount = manager->mountModule();
            if (!mount)
            {
                error = "Mount module not available";
                return {};
            }
            const int idx = args[QStringLiteral("index")].toInt();
            if (idx < 0)
            {
                error = "index must be >= 0";
                return {};
            }
            if (!mount->setSlewRate(idx))
            {
                error = "Driver rejected setSlewRate";
                return {};
            }
            return QJsonObject { { QStringLiteral("success"), true } };
        }
    });

    // -----------------------------------------------------------------------
    // mount_get_slew_rates
    // -----------------------------------------------------------------------
    registry->registerTool(
    {
        QStringLiteral("mount_get_slew_rates"),
        QStringLiteral("Return the list of slew rates supported by the driver, and the currently selected index."),
        {},
        [manager](const QJsonObject &, QString & error) -> QJsonValue
        {
            auto *mount = manager->mountModule();
            if (!mount || !mount->activeMount())
            {
                error = "Mount not available";
                return {};
            }
            QJsonArray names;
            for (const auto &n : mount->activeMount()->slewRates())
                names.append(n);
            return QJsonObject {
                { QStringLiteral("rates"),   names },
                { QStringLiteral("current"), mount->slewRate() }
            };
        }
    });

    registry->classify(QStringLiteral("mount_coords"),            /*ro*/true,  /*destr*/false, /*idemp*/true);
    registry->classify(QStringLiteral("mount_goto"),              /*ro*/false, /*destr*/false, /*idemp*/false);
    registry->classify(QStringLiteral("mount_goto_target"),       /*ro*/false, /*destr*/false, /*idemp*/false);
    registry->classify(QStringLiteral("mount_sync"),              /*ro*/false, /*destr*/true,  /*idemp*/false);
    registry->classify(QStringLiteral("mount_park"),              /*ro*/false, /*destr*/false, /*idemp*/true);
    registry->classify(QStringLiteral("mount_unpark"),            /*ro*/false, /*destr*/false, /*idemp*/true);
    registry->classify(QStringLiteral("mount_abort"),             /*ro*/false, /*destr*/false, /*idemp*/true);
    registry->classify(QStringLiteral("mount_set_meridian_flip"), /*ro*/false, /*destr*/false, /*idemp*/true);
    registry->classify(QStringLiteral("mount_set_tracking"),      /*ro*/false, /*destr*/false, /*idemp*/true);
    registry->classify(QStringLiteral("mount_set_track_mode"),    /*ro*/false, /*destr*/false, /*idemp*/true);
    registry->classify(QStringLiteral("mount_set_slew_rate"),     /*ro*/false, /*destr*/false, /*idemp*/true);
    registry->classify(QStringLiteral("mount_get_slew_rates"),    /*ro*/true,  /*destr*/false, /*idemp*/true);
}

} // namespace MCP::Tools
