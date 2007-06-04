/***************************************************************************
                          constellationboundarycomponent.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/10/08
    copyright            : (C) 2005 by Thomas Kabelmann
    email                : thomas.kabelmann@gmx.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef CONSTELLATIONBOUNDARYCOMPONENT_H
#define CONSTELLATIONBOUNDARYCOMPONENT_H

#include <QList>
#include <QChar>
#include <QPolygonF>

#include "linelistcomponent.h"

class KStarsData;

/**
	*@class ConstellationBoundaryComponent
	*Represents the constellation lines on the sky map.

	*@author Jason Harris
	*@version 0.1
	*/
class ConstellationBoundaryComponent : public LineListComponent
{
	public:
		
	/**
		*@short Constructor
		*@p parent Pointer to the parent SkyComponent object
		*/
		ConstellationBoundaryComponent(SkyComponent *parent, bool (*visibleMethod)());
	/**
		*@short Destructor.  Delete list members
		*/
		~ConstellationBoundaryComponent();
		
	/**
		*@short Initialize the Constellation boundary component
		*Reads the constellation boundary data from cbounds.dat.  
		*The boundary data is defined by a series of RA,Dec coordinate pairs 
		*defining the "nodes" of the boundaries.  The nodes are organized into 
		*"segments", such that each segment represents a continuous series 
		*of boundary-line intervals that divide two particular constellations.
		*
		*The definition of a segment begins with an integer describing the 
		*number of Nodes in the segment.  This is followed by that number of 
		*RA,Dec pairs (RA in hours, Dec in degrees).  Finally, there is an integer 
		*indicating the number of constellations bordered by this segment (this 
		*number is always 2), followed by the IAU abbreviations of the two 
		*constellations.
		*
		*Since the definition of a segment can span multiple lines, we parse this 
		*file word-by-word, rather than line-by-line as we do in other files.
		*
		*@p data Pointer to the KStarsData object
		*/
		virtual void init(KStarsData *data);
	
		QPolygonF boundary( const QString &name ) const;

		QString constellation( SkyPoint *p );
		bool inConstellation( const QString &name, SkyPoint *p );

	private:
		QHash<QString, QPolygonF> Boundary;
};

#endif
