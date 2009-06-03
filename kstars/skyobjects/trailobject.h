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

#include "skyobject.h"


#define MAXTRAIL 400  //maximum number of points in a planet trail

/**
 *@class TrailObject
 *@short provides a SkyObject with an attachable Trail
 *@author Jason Harris
 *@version 1.0
 */
class TrailObject : public SkyObject {
public:
    /**
     *Constructor
     */
    explicit TrailObject( int t=TYPE_UNKNOWN, dms r=dms(0.0), dms d=dms(0.0), float m=0.0, const QString &n=QString() );

    /**
     *Constructor
     */
    TrailObject( int t, double r, double d, float m=0.0, const QString &n=QString() );

    virtual TrailObject* clone() const;
    
    /**
     *@return whether the planet has a trail
     */
    inline bool hasTrail() const { return ( Trail.count() > 0 ); }

    /**
     *@return a reference to the planet's trail
     */
    inline QList<SkyPoint>& trail() { return Trail; }

    /**
     *@short adds a point to the planet's trail
     */
    inline void addToTrail() { Trail.append( SkyPoint( ra(), dec() ) ); }

    /**
     *@short removes the oldest point from the trail
     */
    inline void clipTrail() { Trail.removeFirst(); }

    /**
     *@short clear the Trail
     */
    inline void clearTrail() { Trail.clear(); }

    /**
     *@short update Horizontal coords of the trail
     */
    void updateTrail( dms *LST, const dms *lat );

    /**
     *@short Show Solar System object popup menu.  
     *@note Overloaded from virtual SkyObject::showPopupMenu()
     *@param pmenu pointer to the KSPopupMenu object
     *@param pos QPojnt holding the x,y coordinates for the menu
     */
    virtual void showPopupMenu( KSPopupMenu *pmenu, const QPoint &pos );


protected:
    QList<SkyPoint> Trail;

};

#endif
