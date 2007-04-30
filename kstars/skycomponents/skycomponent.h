/***************************************************************************
                          skycomponent.h  -  K Desktop Planetarium
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

#ifndef SKYCOMPONENT_H
#define SKYCOMPONENT_H


#include <kdebug.h>

class QPainter;
class QString;

class KSNumbers;
class KStars;
class KStarsData;
class SkyObject;
class SkyPoint;

	/**
		*@short static placeholder function for SkyComponents that don't 
		*need a visibility function.  Simply returns true.
		*/
	static bool alwaysVisible() { return true; }

/**
	*@class SkyComponent
	*SkyComponent represents an object on the sky map. This may be a 
	*star, a planet or an imaginary line like the equator.
	*The SkyComponent uses the composite design pattern.  So if a new 
	*object type is needed, it has to be derived from this class and 
	*must be added into the hierarchy of components.
	*((TODO_DOX: Add outline of Components hierarchy))
	*
	*@author Thomas Kabelmann
	*@version 0.1
	*/
class SkyComponent
{
	public:
	
		/**
			*@short Constructor
			*@p parent pointer to the parent SkyComponent
			*@p visibleMethod pointer to the function which determines 
			*whether this component should be drawn in the map.  Defaults
			*to always visible.
			*/
		explicit SkyComponent( SkyComponent *parent, bool (*visibleMethod)() = &alwaysVisible );
		
		/**
			*@short Destructor
			*/
		virtual ~SkyComponent();
		
		/**
			*@short Draw the object on the SkyMap
			*@p ks Pointer to the KStars object
			*@p psky Reference to the QPainter on which to paint
			*@p scale the scaling factor for drawing (1.0 for screen draws)
			*/
		virtual void draw( KStars*, QPainter&, double ) {}
		
		/**
			*@short Draw the object, if it is exportable to an image
			*@p ks Pointer to the KStars object
			*@p psky Reference to the QPainter on which to paint
			*@p scale the scaling factor for drawing (1.0 for screen draws)
			*@see isExportable()
			*/
		void drawExportable(KStars *ks, QPainter& psky, double scale);

		virtual void drawPlanets(KStars * /*ks*/, QPainter& /*psky*/, double /*scale*/) {}
		
		/**
			*@short Initialize the component - load data from disk etc.
			*@p data Pointer to the KStarsData object
			*/
		virtual void init(KStarsData */*data*/) { kDebug() << "Should never see this" << endl; }
		
		/**
			*@short Update the sky position(s) of this component.
			*
			*This function usually just updates the Horizontal (Azimuth/Altitude)
			*coordinates of its member object(s).  However, the precession and
			*nutation must also be recomputed periodically.  Requests to do so are
			*sent through the doPrecess parameter.
			*@p data Pointer to the KStarsData object
			*@p num Pointer to the KSNumbers object
			*@note this is a pure virtual function, it must be reimplemented 
			*by the subclasses of SkyComponent.
			*@sa SingleComponent::update()
			*@sa ListComponent::update()
			*@sa ConstellationBoundaryComponent::update()
			*/
		virtual void update( KStarsData*, KSNumbers* ) {}
		virtual void updatePlanets( KStarsData *, KSNumbers * ) {}
		virtual void updateMoons( KStarsData *, KSNumbers * ) {}
	
		/**
			*@return true if the component is to be drawn on the map.
			*
			*This meta-function is actually a pointer to the appropriate
			*Options::showXXX() function for this component.  It allows
			*us to call the appropriate function by just calling 
			*"visible()".
			*/
		bool (*visible)();

		/**
			*The parent of a component may be a composite or nothing.
			*It's useful to know it's parent, if a component want to 
			*add a component to it's parent, for example a star want 
			*to add/remove a trail to it's parent.
			*/
		SkyComponent* parent() { return Parent; }
		
		/**
			*@short Add a Trail to the specified SkyObject.
			*
			*The idea behind this function is that when a trail needs 
			*to be added to SkyObject o, we will simply call 
			*skyComponents()->addTrail( o ), which will then loop through 
			*all sub-components until o is found, and then add a Trail to it.
			*
			*@p o Pointer to the SkyObject to which a Trail will be added
			*@return true if the Trail was successfully added.
			*@note This base function simply returns false, because you 
			*can only add Trails to solar system bodies.
			*@sa SolarSystemSingleComponent::addTrail()
			*@sa SolarSystemListComponent::addTrail()
			*/
		virtual bool addTrail( SkyObject *o );
		virtual bool hasTrail( SkyObject *o, bool &found );
		virtual bool removeTrail( SkyObject *o );
		virtual void clearTrailsExcept( SkyObject *o );
		virtual void drawTrails( KStars *, QPainter &, double );

		virtual SkyObject* first() { return 0; }
		virtual SkyObject* next() { return 0; }

		/**
			*@short Search the children of this SkyComponent for 
			*a SkyObject whose name matches the argument
			*@p name the name to be matched
			*@return a pointer to the SkyObject whose name matches
			*the argument, or a NULL pointer if no match was found.
			*@note This function simply returns the NULL pointer; it 
			*is reimplemented in various sub-classes
			*@sa SkyComposite::findByName()
			*@sa SingleComponent::findByName()
			*@sa ListComponent::findByName()
			*@sa DeepSkyComponent::findByName()
			*/
		virtual SkyObject* findByName( const QString &name );

		/**
			*@short Find the SkyObject nearest the given SkyPoint
			*
			*Look for a SkyObject that is nearer to point p than maxrad.
			*If one is found, then maxrad is reset to the separation of the new nearest object.
			*@p p pointer to the SkyPoint to search around
			*@p maxrad reference to current search radius
			*@return a pointer to the nearest SkyObject
			*@note This function simply returns a NULL pointer; it is
			*reimplemented in various sub-classes.
			*@sa SkyComposite::objectNearest()
			*@sa SingleComponent::objectNearest()
			*@sa ListComponent::objectNearest()
			*@sa DeepSkyComponent::objectNearest()
			*/
		virtual SkyObject* objectNearest( SkyPoint *p, double &maxrad );

		virtual void emitProgressText( const QString &message );

		virtual QStringList& objectNames() { return parent()->objectNames(); }

	protected:

		/**
		 *@return true if the component is exportable to the SkyMap.
		 *@note The default implementation simply returns true. 
		 *Override the function if the object should not 
		 *be exported.
		 */
		virtual bool isExportable();
		
	private:
	
		SkyComponent *Parent;
};

#endif
