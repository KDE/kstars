/***************************************************************************
                          skycomposite.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/07/08
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

#ifndef SKYCOMPOSITE_H
#define SKYCOMPOSITE_H

#include <QList>

#include "skycomponent.h"

class KStars;
class KStarsData;
class GeoLocation;
class KSNumbers;

/**
	*@class SkyComposite
	*SkyComposite is a kind of container class for SkyComponent 
	*objects.  The SkyComposite is responsible for distributing calls 
	*to functions like draw(), init(), and update() to its children,
	*which can be SkyComponents or other SkyComposites with their 
	*own children.  This is based on the "composite/component"
	*design pattern.
	*
	*Performance tuning: Sometimes it will be better to override a 
	*virtual function and do the work in the composite instead of 
	*delegating the request to all sub components.
	*
	*@author Thomas Kabelmann
	*@version 0.1
	*/
class SkyComposite : public SkyComponent
{
	public:

		/**
			*@short Constructor
			*@p parent pointer to the parent SkyComponent
			*/
		SkyComposite( SkyComponent *parent );
		
		/**
			*@short Destructor
			*/
		~SkyComposite();
		
		/**
			*@short Delegate draw requests to all sub components
			*@p ks Pointer to the KStars object
			*@p psky Reference to the QPainter on which to paint
			*@p scale the scaling factor for drawing (1.0 for screen draws)
			*/
		virtual void draw(KStars *ks, QPainter& psky, double scale);

		/**
			*@short Delegate drawExportable requests to all sub 
			*components.
			*@p ks Pointer to the KStars object
			*@p psky Reference to the QPainter on which to paint
			*@p scale the scaling factor for drawing (1.0 for screen draws)
			*/
		virtual void drawExportable(KStars *ks, QPainter& psky, double scale);
		
		/**
			*@short Delegate init requests to all sub components
			*@p data Pointer to the KStarsData object
			*/
		virtual void init(KStarsData *data);
	
		/**
			*@short Delegate update-position requests to all sub components
			*
			*This function usually just updates the Horizontal (Azimuth/Altitude)
			*coordinates.  However, the precession and nutation must also be 
			*recomputed periodically.  Requests to do so are sent through the
			*doPrecess parameter.
			*@p data Pointer to the KStarsData object
			*@p num Pointer to the KSNumbers object
			*@sa updatePlanets()
			*@sa updateMoons()
			*@note By default, the num parameter is NULL, indicating that 
			*Precession/Nutation computation should be skipped; this computation 
			*is only occasionally required.
			*/
		virtual void update(KStarsData *data, KSNumbers *num=0 );
		
		/**
			*@short Delegate planet position updates to the SolarSystemComposite
			*
			*Planet positions change over time, so they need to be recomputed 
			*periodically, but not on every call to update().  This function 
			*will recompute the positions of all solar system bodies except the 
			*Earth's Moon and Jupiter's Moons (because these objects' positions 
			*change on a much more rapid timescale).
			*@p data Pointer to the KStarsData object
			*@p num Pointer to the KSNumbers object
			*@sa update()
			*@sa updateMoons()
			*@sa SolarSystemComposite::updatePlanets()
			*/
		virtual void updatePlanets( KStarsData *data, KSNumbers *num );

		/**
			*@short Delegate moon position updates to the SolarSystemComposite
			*
			*Planet positions change over time, so they need to be recomputed 
			*periodically, but not on every call to update().  This function 
			*will recompute the positions of the Earth's Moon and Jupiter's four
			*Galilean moons.  These objects are done separately from the other 
			*solar system bodies, because their positions change more rapidly,
			*and so updateMoons() must be called more often than updatePlanets().
			*@p data Pointer to the KStarsData object
			*@p num Pointer to the KSNumbers object
			*@sa update()
			*@sa updatePlanets()
			*@sa SolarSystemComposite::updateMoons()
			*/
		virtual void updateMoons( KStarsData *data, KSNumbers *num );

		/**
			*@short Add a new sub component to the composite
			*@p comp Pointer to the SkyComponent to be added
			*/
		void addComponent(SkyComponent *comp);
		
		/**
			*@short Remove a sub component from the composite
			*@p comp Pointer to the SkyComponent to be removed.
			*/
		void removeComponent(SkyComponent *comp);
		
		/**
			*@short Add a Trail to the specified SkyObject
			*Loop over all child SkyComponents; if the SkyObject
			*in the argument is found, add a Trail to it.
			*@p o Pointer to the SkyObject to which a Trail will be added
			*@return true if the object was found and a Trail was added 
			*/
		virtual bool addTrail( SkyObject *o );
		virtual bool hasTrail( SkyObject *o, bool &found );
		virtual bool removeTrail( SkyObject *o );

		/**
			*@short Search the children of this SkyComposite for 
			*a SkyObject whose name matches the argument.
			*
			*The objects' primary, secondary and long-form names will 
			*all be checked for a match.
			*@p name the name to be matched
			*@return a pointer to the SkyObject whose name matches
			*the argument, or a NULL pointer if no match was found.
			*/
		virtual SkyObject* findByName( const QString &name );

		/**
			*@short Identify the nearest SkyObject to the given SkyPoint,
			*among the children of this SkyComposite
			*@p p pointer to the SkyPoint around which to search.
			*@p maxrad reference to current search radius 
			*@return a pointer to the nearest SkyObject
			*/
		virtual SkyObject* objectNearest( SkyPoint *p, double &maxrad );

		QList<SkyComponent*>& components() { return m_Components; }

	private:
		QList<SkyComponent*> m_Components;
};

#endif
