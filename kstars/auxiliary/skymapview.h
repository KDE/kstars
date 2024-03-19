/*
    SPDX-FileCopyrightText: 2024 Akarsh Simha <akarsh@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QString>
#include <QJsonObject>
#include <QList>
#include <optional>

/**
 * @struct SkyMapView
 * Carries parameters of a sky map view
 */
struct SkyMapView
{
    explicit SkyMapView(const QString &name_, const QJsonObject &jsonData);
    SkyMapView() = default;
    QJsonObject toJson() const;

    QString name; // Name of this view (can be empty)
    bool useAltAz; // Mount type is alt-az when true
    double viewAngle; // Focuser rotation in degrees, within [-90°, 90°]
    bool mirror; // Mirrored left-right
    bool inverted; // 180° rotation if true
    double fov; // fov in degrees, NaN when disabled
    bool erectObserver; // Erect observer correction
};

/**
 * @class SkyMapViewManager
 * Manages a list of sky map views
 * @author Akarsh Simha
 * @version 1.0
 */
class SkyMapViewManager
{
    public:
        /** @short Read the list of views from the database */
        static const QList<SkyMapView> &readViews();

        /** @short Drop the list */
        static void drop();

        /** @short Add a view */
        inline static void addView(const SkyMapView &newView)
        {
            m_views.append(newView);
        }

        /** @short Remove a view
         * Note: This is currently an O(N) operation
         */
        static bool removeView(const QString &name);

        /**
         * @short Get the view with the given name
         * @note This is currently an O(N) operation
         */
        static std::optional<SkyMapView> viewNamed(const QString &name);

        /** @short Get the list of available views */
        static const QList<SkyMapView> &getViews()
        {
            return m_views;
        }

        /** @short Commit the list of views to the database */
        static bool save();

    private:
        SkyMapViewManager() = default;
        ~SkyMapViewManager() = default;
        static QList<SkyMapView> m_views;

        /** @short Fill list with standard views */
        static QList<SkyMapView> defaults();
};
