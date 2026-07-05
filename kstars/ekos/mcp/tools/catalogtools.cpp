/*
    SPDX-FileCopyrightText: 2026 Thomas Nemer <thomas.nemer@fortytwo.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "catalogtools.h"

#include "../mcptoolregistry.h"
#include "kstarsdata.h"
#include "skycomponents/skymapcomposite.h"
#include "skyobjects/skyobject.h"
#include "catalogobject.h"
#include "catalogsdb.h"
#include "dialogs/finddialog.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace MCP::Tools
{

namespace
{
// One de-duplicated, magnitude-rankable search result. Built from either an
// in-memory SkyObject (objectLists) or a CatalogObject pulled from the DSO DB.
struct Hit
{
    QString name;
    QString type;
    double ra;   // J2000 hours
    double dec;  // J2000 degrees
    double mag;
};
}

void initCatalogTools(MCP::ToolRegistry *registry)
{
    registry->registerTool(
    {
        QStringLiteral("catalog_search"),
        QStringLiteral("Search KStars catalogs by name. Returns matching sky objects with canonical names "
                       "(suitable for mount_goto_target), SkyObject types, J2000 RA (hours) and Dec (degrees), "
                       "and visual magnitude. Use this to disambiguate user-supplied names before slewing — "
                       "mount_goto_target requires the exact canonical name (\"M 42\", not \"M42\")."),
        {
            {
                QStringLiteral("query"),      QStringLiteral("string"),
                QStringLiteral("Name fragment or full name to search for. Case-insensitive."), true
            },
            {
                QStringLiteral("maxResults"), QStringLiteral("integer"),
                QStringLiteral("Maximum number of matches to return. Default 20, capped at 100."), false
            },
            {
                QStringLiteral("exactMatch"), QStringLiteral("boolean"),
                QStringLiteral("Require an exact (case-insensitive) name match instead of substring. Default false."), false
            },
            {
                QStringLiteral("typeFilter"), QStringLiteral("string"),
                QStringLiteral("Optional SkyObject type label to restrict the search (e.g. \"Galaxy\", \"Open Cluster\", \"Star\", \"Nebula\")."), false
            },
            {
                QStringLiteral("maxMagnitude"), QStringLiteral("number"),
                QStringLiteral("Optional faintest visual magnitude to include (larger = fainter). Objects fainter "
                               "than this, or with unknown magnitude, are excluded. Default: no limit."), false
            }
        },
        [](const QJsonObject & args, QString & error) -> QJsonValue
        {
            QString query = args[QStringLiteral("query")].toString();
            if (query.isEmpty())
            {
                error = "query must not be empty";
                return {};
            }

            // Normalise catalog designations the same way the KStars Find dialog
            // does: "M42" -> "M 42", "NGC5139" -> "NGC 5139", "c20xx" -> "c/20xx".
            query = FindDialog::processSearchText(query);

            int maxResults = args.contains(QStringLiteral("maxResults"))
                             ? args[QStringLiteral("maxResults")].toInt()
                             : 20;
            maxResults = std::clamp(maxResults, 1, 100);

            const bool exactMatch = args[QStringLiteral("exactMatch")].toBool(false);
            const QString typeFilter = args[QStringLiteral("typeFilter")].toString();
            const bool hasMagLimit = args.contains(QStringLiteral("maxMagnitude"));
            const double magLimit = args[QStringLiteral("maxMagnitude")].toDouble();
            const QString lcQuery = query.toLower();

            auto *composite = KStarsData::Instance() ? KStarsData::Instance()->skyComposite() : nullptr;
            if (!composite)
            {
                error = "Sky catalog not available";
                return {};
            }

            std::vector<Hit> hits;
            bool truncated = false;

            // Type filter is applied uniformly against SkyObject::typeName()
            // (the human-readable label, "Galaxy" / "Open Cluster", etc.).
            auto typeAllowed = [&](const QString & type)
            {
                return typeFilter.isEmpty() || type.compare(typeFilter, Qt::CaseInsensitive) == 0;
            };

            auto addHit = [&](const QString & name, const SkyObject * obj)
            {
                const double mag = obj->mag();
                if (hasMagLimit && (std::isnan(mag) || mag > magLimit))
                    return;
                hits.push_back({ name, obj->typeName(), obj->ra0().Hours(), obj->dec0().Degrees(), mag });
            };

            // Source A — objects already held in memory. The composite's
            // objectLists() is keyed by SkyObject::TYPE; each value is a
            // QVector<QPair<QString name, const SkyObject*>>. This covers named
            // stars, the solar system, supernovae, satellites and comets, but
            // only the handful of DSOs currently loaded/drawn.
            const auto &lists = composite->objectLists();
            for (auto it = lists.constBegin(); it != lists.constEnd(); ++it)
            {
                for (const auto &entry : it.value())
                {
                    const QString &name = entry.first;
                    const SkyObject *obj = entry.second;
                    if (!obj)
                        continue;

                    const bool nameMatches = exactMatch
                                             ? name.compare(query, Qt::CaseInsensitive) == 0
                                             : name.toLower().contains(lcQuery);
                    if (nameMatches && typeAllowed(obj->typeName()))
                        addHit(name, obj);
                }
            }

            // Source B — the full DSO catalog. Galaxies, nebulae and clusters
            // live in the SQLite catalog DB, not objectLists(); they must be
            // queried through CatalogsDB::DBManager (mirrors EkosLive's
            // m_DSOManager). find_objects_by_name() does a substring match on
            // name and long_name ordered by ascending magnitude. The manager is
            // built once and reused; it is internally mutex-guarded.
            static CatalogsDB::DBManager dsoManager{ CatalogsDB::dso_db_path() };
            const auto dsoMatches = dsoManager.find_objects_by_name(query, maxResults, exactMatch);
            if (static_cast<int>(dsoMatches.size()) >= maxResults)
                truncated = true;
            for (const auto &obj : dsoMatches)
            {
                if (typeAllowed(obj.typeName()))
                    addHit(obj.name(), &obj);
            }

            // Brightest first — deterministic ordering across both sources.
            // Unknown magnitudes (NaN) sort last rather than at an arbitrary spot.
            auto rank = [](double m)
            {
                return std::isnan(m) ? std::numeric_limits<double>::infinity() : m;
            };
            std::stable_sort(hits.begin(), hits.end(),
                             [&](const Hit & a, const Hit & b)
            {
                return rank(a.mag) < rank(b.mag);
            });

            QJsonArray matches;
            QSet<QString> seen;  // de-dup loaded DSOs that appear in both sources
            for (const auto &hit : hits)
            {
                const QString key = hit.name.toLower();
                if (seen.contains(key))
                    continue;

                if (matches.size() >= maxResults)
                {
                    truncated = true;
                    break;
                }
                seen.insert(key);

                matches.append(QJsonObject
                {
                    { QStringLiteral("name"),      hit.name },
                    { QStringLiteral("type"),      hit.type },
                    { QStringLiteral("ra"),        hit.ra },
                    { QStringLiteral("dec"),       hit.dec },
                    { QStringLiteral("magnitude"), hit.mag }
                });
            }

            return QJsonObject
            {
                { QStringLiteral("matches"),   matches },
                { QStringLiteral("truncated"), truncated }
            };
        }
    });

    registry->classify(QStringLiteral("catalog_search"), /*ro*/true, /*destr*/false, /*idemp*/true);
}

} // namespace MCP::Tools
