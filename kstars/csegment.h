/***************************************************************************
                          csegment.h  -  K Desktop Planetarium
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

#ifndef CSEGMENT_H
#define CSEGMENT_H

#include <qstring.h>
#include <qptrlist.h>
#include "skypoint.h"

class CSegment {
public:
	CSegment();
	~CSegment() {};
	void addPoint( double ra, double dec );
	bool setNames( QString n1, QString n2 );
	bool borders( QString cname );

	SkyPoint* firstNode() { return Nodes.first(); }
	SkyPoint* nextNode() { return Nodes.next(); }

	QPtrList<SkyPoint>* nodes() { return &Nodes; }

private:
	QPtrList<SkyPoint> Nodes;
	QString Name1, Name2;
};



#endif
