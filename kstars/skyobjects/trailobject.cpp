/***************************************************************************
                          trailobject.cpp  -  K Desktop Planetarium
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

#include "trailobject.h"
#include "kspopupmenu.h"

TrailObject::TrailObject( int t, dms r, dms d, float m, const QString &n ) 
  : SkyObject( t, r, d, m, n )
{}

TrailObject::TrailObject( int t, double r, double d, float m, const QString &n ) 
  : SkyObject( t, r, d, m, n )
{}

TrailObject* TrailObject::clone() const {
    return new TrailObject(*this);
}

void TrailObject::updateTrail( dms *LST, const dms *lat ) {
    for ( int i=0; i < Trail.size(); ++i )
        Trail[i].EquatorialToHorizontal( LST, lat );
}

void TrailObject::showPopupMenu( KSPopupMenu *pmenu, const QPoint &pos ) {
    pmenu->createPlanetMenu( this ); pmenu->popup( pos );
}

