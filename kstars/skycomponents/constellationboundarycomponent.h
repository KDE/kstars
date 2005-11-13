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

#include "skycomponent.h"

/**
	*@class ConstellationBoundaryComponent
	*Represents the constellation lines on the sky map.

	*@author Jason Harris
	*@version 0.1
	*/

class SkyComposite;
class KStarsData;
class SkyMap;
class KSNumbers;
class CSegment;

#include <QList>
#include <QChar>

//CBComponent doesn't fit into any of the three base-class Components
//It comes closest to PointListComponent, but its members are CSegment objects,
//not SkyPoints.  Since CSegments are essentially themselves collections of
//SkyPoints, we could make this CBComposite, with PointListComponent
//members.  However, a CSegment is a bit more than a list of points; it also 
//has two constell. names associated with it.  So, it's easiest to just 
//make this class inherit SkyComponent directly, so we can define the unique 
//QList<CSegment> member
class ConstellationBoundaryComponent : public SkyComponent
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
		*@short Draw constellation boundaries on the sky map.
		*@p ks pointer to the KStars object
		*@p psky Reference to the QPainter on which to paint
		*@p scale scaling factor (1.0 for screen draws)
		*/
		virtual void draw( KStars *ks, QPainter& psky, double scale );

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
	
		/**
			*@short Update the sky position(s) of this component.
			*
			*This function usually just updates the Horizontal (Azimuth/Altitude)
			*coordinates of its member object(s).  However, the precession and
			*nutation must also be recomputed periodically.  Requests to do so are
			*sent through the doPrecess parameter.
			*@p data Pointer to the KStarsData object
			*@p num Pointer to the KSNumbers object
			*@note By default, the num parameter is NULL, indicating that 
			*Precession/Nutation computation should be skipped; this computation 
			*is only occasionally required.
			*@sa SingleComponent::update()
			*@sa ListComponent::update()
			*/
		virtual void update( KStarsData *data, KSNumbers *num=0 );
		
		QList<CSegment*>& segmentList();

	private:
		QList<CSegment*> m_SegmentList;
};

#endif
