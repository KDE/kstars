/***************************************************************************
                          csegment.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Sun Feb 1 2004
    copyright            : (C) 2004 by Jason Harris
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

#include "csegment.h"
#include "skypoint.h"

CSegment::CSegment() : Name1(), Name2() {
	Nodes.setAutoDelete( true );
}

void CSegment::addPoint( double ra, double dec ) {
	SkyPoint *p = new SkyPoint( ra, dec );
	Nodes.append( p );
}

bool CSegment::setNames( QString n1, QString n2 ) {
	if ( n1.length() == 3 && n2.length() == 3 ) {
		Name1 = n1; 
		Name2 = n2;
		return true;
	} else {
		return false;
	}
}

bool CSegment::borders( QString cname ) {
	if ( Name1 == cname || Name2 == cname ) return true;
	return false;
}
