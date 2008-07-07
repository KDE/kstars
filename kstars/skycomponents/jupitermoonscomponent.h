/***************************************************************************
                          jupitermoonscomponent.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/13/08
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

#ifndef JUPITERMOONSCOMPONENT_H
#define JUPITERMOONSCOMPONENT_H

#include "skycomponent.h"

class SkyComposite;
class SolarSystemSingleComponent;
class KStarsData;
class SkyMap;
class KSNumbers;
class JupiterMoons;
class SkyLabeler;

/**
	*@class JupiterMoonsComponent
	*Represents the jupitermoons on the sky map.
	
	*@author Thomas Kabelmann
	*@version 0.1
	*/
class JupiterMoonsComponent : public SkyComponent
{
public:

    /**
     *@short Constructor
     *@p parent pointer to the parent SkyComposite
     */
    JupiterMoonsComponent( SkyComponent *parent, SolarSystemSingleComponent *jup, bool (*visibleMethod)() );

    /**
     *@short Destructor
     */
    virtual ~JupiterMoonsComponent();

    /**
     *@short Draw the Jovian moons on the sky map
     *@p map Pointer to the SkyMap object
     *@p psky Reference to the QPainter on which to paint
     */
    virtual void draw( QPainter& psky );

    /**
     *@short Initialize the Jovian moons
     */
    virtual void init();

    virtual void update( KSNumbers *num );
    virtual void updateMoons( KSNumbers *num );

    virtual SkyObject* objectNearest( SkyPoint *p, double &maxrad );

    /**
    	*@return a pointer to a moon if its name matches the argument
    	*
    	*@p name the name to be matched
    	*@return a SkyObject pointer to the moon whose name matches
    	*the argument, or a NULL pointer if no match was found.
    	*/
    virtual SkyObject* findByName( const QString &name );

protected:
    /**
    	*@short Draws the moons' trails, if necessary.
    	*/
    void drawTrails( QPainter& psky );

    /**
    	*@short Add a Trail to the specified SkyObject.
    	*@p o Pointer to the SkyObject to which a Trail will be added
    	*/
    virtual bool addTrail( SkyObject *o );

    /**
    	*@return true if the specified SkyObject is a member of this component, and it contains a Trail.
    	*@p o Pointer to the SkyObject to which a Trail will be added
    	*/
    virtual bool hasTrail( SkyObject *o, bool& found );
    virtual bool removeTrail( SkyObject *o );
    virtual void clearTrailsExcept( SkyObject *exOb );

private:

    JupiterMoons *jmoons;
    SolarSystemSingleComponent *m_Jupiter;
};

#endif
