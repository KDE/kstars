/*
    SPDX-FileCopyrightText: 2007 Jason Harris <kstars@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QSet>

#include "skyobject.h"

class SkyPainter;

/**
 *@class TrailObject
 *@short provides a SkyObject with an attachable Trail
 *@author Jason Harris
 *@version 1.0
 */
class TrailObject : public SkyObject
{
    public:
        /** Constructor */
        explicit TrailObject(int t = TYPE_UNKNOWN, dms r = dms(0.0), dms d = dms(0.0), float m = 0.0,
                             const QString &n = QString());

        /** Constructor */
        TrailObject(int t, double r, double d, float m = 0.0, const QString &n = QString());

        ~TrailObject() override;

        TrailObject *clone() const override;

        /** @return whether the planet has a trail */
        inline bool hasTrail() const
        {
            return (Trail.count() > 0);
        }

        /** @return a reference to the planet's trail */
        inline const QList<SkyPoint> &trail() const
        {
            return Trail;
        }

        /** @short adds a point to the planet's trail */
        void addToTrail(const QString &label = QString());

        /** @short removes the oldest point from the trail */
        void clipTrail();

        /** @short clear the Trail */
        void clearTrail();

        /** @short update Horizontal coords of the trail */
        void updateTrail(dms *LST, const dms *lat);

        /**Remove trail for all objects but one which is passed as
             * parameter. It has SkyObject type for generality. */
        static void clearTrailsExcept(SkyObject *o);

        void drawTrail(SkyPainter *skyp) const;

        /** Maximum trail size */
        static const int MaxTrail = 400;

        void initPopupMenu(KSPopupMenu *pmenu) override;

    protected:
        QList<SkyPoint> Trail;
        QList<QString> m_TrailLabels;
        /// Store list of objects with trails.
        static QSet<TrailObject *> trailObjects;

    private:
};

