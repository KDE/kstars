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

class QPainter;

class SkyMap;
class KStarsData;
class SkyComposite;
class KStarsData;
class KSNumbers;
class SkyObject;

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
			*@p parent pointer to the parent SkyComposite
			*/
		SkyComponent(SkyComposite *parent);
		
		/**
			*@short Destructor
			*/
		virtual ~SkyComponent();
		
		/**
			*@short Draw the object on the SkyMap
			*@p map Pointer to the SkyMap object
			*@p psky Reference to the QPainter on which to paint
			*@p scale the scaling factor for drawing (1.0 for screen draws)
			*/
		virtual void draw( SkyMap*, QPainter&, double ) {};
		
		/**
			*Draw the object, if it is exportable to an image
			*@p map Pointer to the SkyMap object
			*@p psky Reference to the QPainter on which to paint
			*@p scale the scaling factor for drawing (1.0 for screen draws)
			*@see isExportable()
			*/
		void drawExportable(SkyMap *map, QPainter& psky, double scale);
		
		/**
			*@short Initialize the component - load data from disk etc.
			*@p data Pointer to the KStarsData object
			*/
		// TODO pass a splashscreen as parameter to init, so it 
		// can update the splashscreen -> should use the new 
		// KSplashScreen?
		virtual void init(KStarsData *data);
		
		/**
			*For optimization there are 3 update methods. Long 
			*operations, like moon updates can be called separately, 
			*so it's not necessary to process it every update call.
			*updatePlanets() is used by planets, comets, ecliptic.
			*updateMoons() is used by the moon and the jupiter moons.
			*update() is used by all components and for updating 
			*Alt/Az coordinates.
			*
			*@p data Pointer to the KStarsData object
			*@p num Pointer to the KSNumbers object
			*@p needNewCoords true if cordinates should be recomputed
			*/
		virtual void updatePlanets( KStarsData*, KSNumbers*, bool ) {};

		virtual void updateMoons( KStarsData*, KSNumbers*, bool ) {};

		virtual void update( KStarsData*, KSNumbers*, bool ) {};
	
		/**
			*The parent of a component may be a composite or nothing.
			*It's useful to know it's parent, if a component want to 
			*add a component to it's parent, for example a star want 
			*to add/remove a trail to it's parent.
			*/
		SkyComposite* parent() { return Parent; }
		
		/**
			*@short Add a Trail to the specified SkyObject.
			*@p o Pointer to the SkyObject to which a Trail will be added
			*@return true if the Trail was successfully added.
			*@note This base function simply returns false, because you 
			*can only add Trails to solar system bodies.  This method 
			*is overridden by the Solar System components
			*/
		bool addTrail( SkyObject *o );

		/**
			*@short Search the children of this SkyComponent for 
			*a SkyObject whose name matches the argument
			*@p name the name to be matched
			*@return a pointer to the SkyObject whose name matches
			*the argument, or a NULL pointer if no match was found.
			*/
		SkyObject* findByName( const QString &name );

	protected:

		/**
		 *@return true if the component is exportable to the SkyMap.
		 *@note The default implementation simply returns true. 
		 *Override the function if the object should not 
		 *be exported.
		 */
		virtual bool isExportable();
		
		/** 
		 *Draws the label of the object.
		 *This is an default implementation and can be overridden 
		 *when needed.  For most cases it's just needed to 
		 *reimplement the labelSize() method.
		 *Currently only stars have their own implementation of 
		 *drawNameLabel().
		 *@p psky Reference to the QPainter on which to draw
		 *@p obj Pointer to the SkyObject to which a label will 
		 *be attached
		 *@p x the pixel x-coordinate of the object
		 *@p y the pixel y-coordinate of the object
		 *@p scale the scaling factor for drawing (1.0 for screen draws)
		 */
		virtual void drawNameLabel(QPainter &psky, SkyObject *obj, int x, int y, double scale);
		
		/** 
		 *@return the size for drawing the label. It's used by 
		 *the generic drawNameLabel() method.
		 *@p obj Pointer to the SkyObject
		 */
		virtual int labelSize(SkyObject*) { return 1; };

	private:
		SkyComposite *Parent;
};

#endif
