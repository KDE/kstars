/***************************************************************************
                          pointlistcomponent.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/10/01
    copyright            : (C) 2005 by Jason Harris
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

#ifndef POINTLISTCOMPONENT_H
#define POINTLISTCOMPONENT_H

/**
 *@class PointListComponent
 *An abstract parent class, to be inherited by SkyComponents that store a QList
 *of SkyPoints.
 *
 *@author Jason Harris
 *@version 0.1
 */

class SkyMap;

#include "skycomponent.h"

class PointListComponent : public SkyComponent
{
	public:
	
		PointListComponent( SkyComposite *parent, bool (*visibleMethod)() );
		
		virtual ~PointListComponent();
		
		/**Draw the list of objects on the SkyMap*/
		virtual void draw(KStars *ks, QPainter& psky, double scale) {};
		
		/**Draw the object, if it is exportable to an image
		*@see isExportable()
		*/
		void drawExportable(KStars *ks, QPainter& psky, double scale);
		
		/**
			*@short Update the sky positions of this component.
			*
			*This function usually just updates the Horizontal (Azimuth/Altitude)
			*coordinates of the objects in this component.  However, the precession
			*and nutation must also be recomputed periodically.  Requests to do
			*so are sent through the doPrecess parameter.
			*@p data Pointer to the KStarsData object
			*@p num Pointer to the KSNumbers object
			*@p doPrecession true if precession/nutation should be recomputed
			*@note reimplemented from SkyComponent.
			*/
		virtual void update( KStarsData *data, KSNumbers *num=0, bool doPrecession=false );
		
		QList<SkyPoint*>& pointList() { return m_PointList; }

	private:
		SkyComposite *Parent;
		QList<SkyPoint*> m_PointList;
};

#endif
