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

#include "skycomponent.h"

class QList;
class SkyMap;
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
		 *@p parent pointer to the parent SkyComposite
		 */
		SkyComposite(SkyComposite *parent);
		
		/**
		 *@short Destructor
		 */
		virtual ~SkyComposite();
		
		/**
		 *@short Delegates draw requests to it's sub components
		 *@p map Pointer to the SkyMap object
		 *@p psky Reference to the QPainter on which to paint
		 *@p scale the scaling factor for drawing (1.0 for screen draws)
		 */
		virtual void draw(SkyMap *map, QPainter& psky, double scale);

		/**
		 *@short Delegates drawExportable requests to it's sub 
		 *components.
		 *@p map Pointer to the SkyMap object
		 *@p psky Reference to the QPainter on which to paint
		 *@p scale the scaling factor for drawing (1.0 for screen draws)
		 */
		virtual void drawExportable(SkyMap *map, QPainter& psky, double scale);
		
		/**
		 *@short Delegates init requests to it's sub components
		 *@p data Pointer to the KStarsData object
		 */
		virtual void init(KStarsData *data);
	
		/**
		 *@short Delegates update requests to it's sub components
		 *@p data Pointer to the KStarsData object
		 *@p num Pointer to the KSNumbers object
		 *@p needNewCoords true if cordinates should be recomputed
		 */
		virtual void update(KStarsData *data, KSNumbers*, bool needNewCoords);
		
		//TODO Optimization: updatePlanets + Moon doesn't need to delegate it's request
		// to all sub components (perhaps auto setting by subcomponents?)
		virtual void updatePlanets(KStarsData*, KSNumbers*, bool needNewCoords) {};

		virtual void updateMoons(KStarsData*, KSNumbers*, bool needNewCoords) {};

		/**
		 *short Add a new sub component to the composite
		 *@p comp Pointer to the SkyComponent to be added
		 */
		void addComponent(SkyComponent *comp);
		
		/**
		 *@short Remove a sub component from the composite
		 *@p comp Pointer to the SkyComponent to be removed.
		 */
		void removeComponent(SkyComponent *comp);
		
		/**
		 *@short Add a Trail to the specified SkyComponent
		 *Loop over all child SkyComponents; if the SkyObject
		 *in the argument is found, add a Trail to it.
		 *@p o Pointer to the SkyObject to which a Trail will be added
		 *@return true if the object was found and a Trail was added 
		 */
		bool addTrail( SkyComponent *o );

		/**
		 *@short Search the children of this SkyComponent for 
		 *a SkyObject whose name matches the argument
		 *@p name the name to be matched
		 *@return a pointer to the SkyObject whose name matches
		 *the argument, or a NULL pointer if no match was found.
		 */
		SkyObject* findByName( const QString &name );

	protected:
		
		/**Returns the list of components (for internal use)*/
		QList<SkyComponent*>* components();
		
	private:
		/**All sub components are stored here*/
		QList<SkyComponent*> *Components;
};

#endif
