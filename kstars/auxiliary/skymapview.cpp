/*
   SPDX-FileCopyrightText: 2024 Akarsh Simha <akarsh@kde.org>

   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "skymapview.h"
#include "nan.h"
#include "kstarsdata.h"
#include "ksuserdb.h"
#include <kstars_debug.h>

SkyMapView::SkyMapView(const QString &name_, const QJsonObject &jsonData)
    : name(name_)
{
    // Check version
    if (jsonData["version"].toString() != "1.0.0")
    {
        qCCritical(KSTARS) << "Unhandled SkyMapView JSON schema version " << jsonData["version"].toString();
        return;
    }
    useAltAz = jsonData["useAltAz"].toBool();
    viewAngle = jsonData["viewAngle"].toDouble();
    mirror = jsonData["mirror"].toBool();
    inverted = jsonData["inverted"].toBool();
    fov = jsonData["fov"].isNull() ? NaN::d : jsonData["fov"].toDouble();
    erectObserver = jsonData["erectObserver"].toBool();
}

QJsonObject SkyMapView::toJson() const
{
    return QJsonObject
    {
        {"version", "1.0.0"},
        {"useAltAz", useAltAz},
        {"viewAngle", viewAngle},
        {"mirror", mirror},
        {"inverted", inverted},
        {"fov", fov},
        {"erectObserver", erectObserver}
    };
}

// // // SkyMapViewManager // // //

QList<SkyMapView> SkyMapViewManager::m_views;

QList<SkyMapView> SkyMapViewManager::defaults()
{
    QList<SkyMapView> views;

    SkyMapView view;

    view.name = i18nc("Set the sky-map view to zenith up", "Zenith Up");
    view.useAltAz = true;
    view.viewAngle = 0;
    view.mirror = false;
    view.inverted = false;
    view.fov = NaN::d;
    view.erectObserver = false;
    views << view;

    view.name = i18nc("Set the sky-map view to zenith down", "Zenith Down");
    view.useAltAz = true;
    view.viewAngle = 0;
    view.mirror = false;
    view.inverted = true;
    view.fov = NaN::d;
    view.erectObserver = false;
    views << view;

    view.name = i18nc("Set the sky-map view to north up", "North Up");
    view.useAltAz = false;
    view.viewAngle = 0;
    view.mirror = false;
    view.inverted = false;
    view.fov = NaN::d;
    view.erectObserver = false;
    views << view;

    view.name = i18nc("Set the sky-map view to north down", "North Down");
    view.useAltAz = false;
    view.viewAngle = 0;
    view.mirror = false;
    view.inverted = true;
    view.fov = NaN::d;
    view.erectObserver = false;
    views << view;

    view.name = i18nc("Set the sky-map view to match a Schmidt-Cassegrain telescope with erecting prism pointed upwards",
                      "SCT with upward diagonal");
    view.useAltAz = true;
    view.viewAngle = 0;
    view.mirror = true;
    view.inverted = false;
    view.fov = NaN::d;
    view.erectObserver = false;
    views << view;

    view.name = i18nc("Set the sky-map view to match the view through a typical Dobsonian telescope", "Typical Dobsonian");
    view.useAltAz = true;
    view.viewAngle = 45;
    view.mirror = false;
    view.inverted = true;
    view.fov = NaN::d;
    view.erectObserver = true;
    views << view;

    return views;
}

bool SkyMapViewManager::save()
{
    // FIXME, this is very inefficient
    bool success = true;
    Q_ASSERT(!!KStarsData::Instance());
    if (!KStarsData::Instance())
    {
        qCCritical(KSTARS) << "Cannot save sky map views, no KStarsData instance.";
        return false;
    }
    KSUserDB *db = KStarsData::Instance()->userdb();
    Q_ASSERT(!!db);
    if (!db)
    {
        qCCritical(KSTARS) << "Cannot save sky map views, no KSUserDB instance.";
        return false;
    }
    success = success && db->DeleteAllSkyMapViews();
    if (!success)
    {
        qCCritical(KSTARS) << "Failed to flush sky map views from the database";
        return success;
    }
    for (const auto &view : m_views)
    {
        bool result = db->AddSkyMapView(view);
        success = success && result;
        Q_ASSERT(result);
        if (!result)
        {
            qCCritical(KSTARS) << "Failed to commit Sky Map View " << view.name << " to the database!";
        }
    }
    return success;
}

const QList<SkyMapView> &SkyMapViewManager::readViews()
{
    Q_ASSERT(!!KStarsData::Instance());
    if (!KStarsData::Instance())
    {
        qCCritical(KSTARS) << "Cannot read sky map views, no KStarsData instance.";
        return m_views;
    }
    KSUserDB *db = KStarsData::Instance()->userdb();
    Q_ASSERT(!!db);
    if (!db)
    {
        qCCritical(KSTARS) << "Cannot save sky map views, no KSUserDB instance.";
        return m_views;
    }
    m_views.clear();
    bool result = db->GetAllSkyMapViews(m_views);
    Q_ASSERT(result);
    if (!result)
    {
        qCCritical(KSTARS) << "Failed to read sky map views from the database!";
    }
    if (m_views.isEmpty())
    {
        m_views = defaults();
    }
    return m_views;
}

void SkyMapViewManager::drop()
{
    m_views.clear();
}

std::optional<SkyMapView> SkyMapViewManager::viewNamed(const QString &name)
{
    for (auto it = m_views.begin(); it != m_views.end(); ++it)
    {
        if (it->name == name)
        {
            return *it;
        }
    }
    return std::nullopt;
}

bool SkyMapViewManager::removeView(const QString &name)
{
    for (auto it = m_views.begin(); it != m_views.end(); ++it)
    {
        if (it->name == name)
        {
            m_views.erase(it);
            return true;
        }
    }
    return false;
}
