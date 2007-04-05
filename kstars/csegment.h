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

/**@class CSegment
	*A segment of a constellation boundary.  The segment consists
	*of two or more SkyPoint "nodes" which are vertices of the 
	*boundary polygon.  A single segment is define as the set of nodes
	*that separates a single pair of constellations.  An entire 
	*constellation boundary must consist of many segments, because 
	*each constellation is surrounded by multiple neighbors.  
	*
	*For example, imagine constellation A is surrounded by constellations
	*B, C, and D.  One CSegment (AB) will describe the boundary between
	*A and B; another (AC) will describe the boundary between A and C;
	*and a third (AD) will describe the boundary between A and D. 
	*/

class SkyPoint;

class CSegment {
public:
	/**Constructor*/
	CSegment();
	/**Destructor (empty)*/
	~CSegment() {}
	
	/**Add a SkyPoint node to the boundary segment.
		*@p ra the RA of the node
		*@p dec the Dec of the node
		*/
	void addPoint( double ra, double dec );
	
	/**@return the name of one of the constellations
		*that borders this boundary segment.
		*/
	QString name1() const { return Name1; }
	
	/**@return the name of one of the constellations
		*that borders this boundary segment.
		*/
	QString name2() const { return Name2; }
	
	/**Set the names of the bounding constellations.  Use the IAU
		*three-letter abbreviations.
		*@p n1 IAU name of one bounding constellation
		*@p n2 IAU name of the other bounding constellation
		*/
	bool setNames( QString n1, QString n2 );
	
	/**Determine if a given constellation borders this boundary segment
		*@p cname the IAU code of the constellation to be tested.
		*/
	bool borders( QString cname );

	/**@return pointer to the first node in the segment
		*/
	SkyPoint* firstNode() { return Nodes.first(); }
	/**@return pointer to the next node in the segment.
		*If we were on the last node, return the NULL pointer.
		*/
	SkyPoint* nextNode() { return Nodes.next(); }

	/**@return pointer to the list of nodes*/
	QPtrList<SkyPoint>* nodes() { return &Nodes; }

private:
	QPtrList<SkyPoint> Nodes;
	QString Name1, Name2;
};

#endif
