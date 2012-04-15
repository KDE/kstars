/***************************************************************************
                          trailobject.h  -  K Desktop Planetarium
                             -------------------
    begin                : Sat Oct 27 2007
    copyright            : (C) 2007 by Jason Harris
    email                : kstars@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef TRAILOBJECT_H_
#define TRAILOBJECT_H_

#include <QSet>

#include "skyobject.h"

class SkyPainter;

/**
 *@class TrailObject
 *@short provides a SkyObject with an attachable Trail
 *@author Jason Harris
 *@version 1.0
 */
class TrailObject : public SkyObject {
public:
    /** Constructor */
    explicit TrailObject( int t=TYPE_UNKNOWN, dms r=dms(0.0), dms d=dms(0.0), float m=0.0, const QString &n=QString() );

    /** Constructor */
    TrailObject( int t, double r, double d, float m=0.0, const QString &n=QString() );

    virtual ~TrailObject();

    virtual TrailObject* clone() const;
    
    /** @return whether the planet has a trail */
    inline bool hasTrail() const { return ( Trail.count() > 0 ); }

    /** @return a reference to the planet's trail */
    inline const QList<SkyPoint>& trail() const { return Trail; }

    /** @short adds a point to the planet's trail */
    void addToTrail();

    /** @short removes the oldest point from the trail */
    void clipTrail();

    /** @short clear the Trail */
    void clearTrail();

    /** @short update Horizontal coords of the trail */
    void updateTrail( dms *LST, const dms *lat );

    /**Remove trail for all objects but one which is passed as
     * parameter. It has SkyObject type for generality. */
    static void clearTrailsExcept(SkyObject* o);

    void drawTrail(SkyPainter *skyp) const;

    /** Maximum trail size */
    static const int MaxTrail = 400;
protected:
    QList<SkyPoint> Trail;
    /// Store list of objects with trails.
    static QSet<TrailObject*> trailObjects;
private:
    virtual void initPopupMenu( KSPopupMenu *pmenu );
};

#endif
