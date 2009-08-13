/***************************************************************************
                          planetmoonscomponent.h  -  K Desktop Planetarium
                             -------------------
     begin                : Sat Mar 13 2009
                          : by Vipul Kumar Singh, Médéric Boquien
     email                : vipulkrsingh@gmail.com, mboquien@free.fr
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef PLANETMOONSCOMPONENT_H
#define PLANETMOONSCOMPONENT_H

#include "skycomponent.h"
#include "skyobjects/ksplanetbase.h"
#include "skyobjects/planetmoons.h"

class SkyComposite;
class SolarSystemSingleComponent;
class KStarsData;
class SkyMap;
class KSNumbers;
class JupiterMoons;
class SaturnMoons;
class SkyLabeler;

/**
	*@class PlanetMoonsComponent
	*Represents the planetmoons on the sky map.
	
	*@author Vipul Kumar Singh
	*@author Médéric boquien
	*@version 0.1
	*/
class PlanetMoonsComponent : public SkyComponent
{
public:
    /**
     *@short Constructor
     *@p parent pointer to the parent SkyComposite
     */
    PlanetMoonsComponent( SkyComponent *parent, SolarSystemSingleComponent *pla, KSPlanetBase::Planets planet, bool (*visibleMethod)() );

    /**
     *@short Destructor
     */
    ~PlanetMoonsComponent();

    /**
     *@short Draw the moons on the sky map
     *@p map Pointer to the SkyMap object
     *@p psky Reference to the QPainter on which to paint
     */
    void draw( QPainter& psky );

    /**
     *@short Initialize the planet moons
     *@p data Pointer to the KStarsData object
     */
    void init(KStarsData *data);

    void update( KStarsData *data, KSNumbers *num );
    void updateMoons( KStarsData *data, KSNumbers *num );

    SkyObject* objectNearest( SkyPoint *p, double &maxrad );

    /**
    	*@return a pointer to a moon if its name matches the argument
    	*
    	*@p name the name to be matched
    	*@return a SkyObject pointer to the moon whose name matches
    	*the argument, or a NULL pointer if no match was found.
    	*/
    SkyObject* findByName( const QString &name );

protected:
    /**
    	*@short Draws the moons' trails, if necessary.
    	*/
    void drawTrails( QPainter& psky );

    /**
    	*@short Add a Trail to the specified SkyObject.
    	*@p o Pointer to the SkyObject to which a Trail will be added
    	*/
    bool addTrail( SkyObject *o );

    /**
    	*@return true if the specified SkyObject is a member of this component, and it contains a Trail.
    	*@p o Pointer to the SkyObject to which a Trail will be added
    	*/
    bool hasTrail( SkyObject *o, bool& found );
    bool removeTrail( SkyObject *o );
    void clearTrailsExcept( SkyObject *exOb );

private:
    KSPlanetBase::Planets planet;
    PlanetMoons *pmoons;
    SolarSystemSingleComponent *m_Planet;
};

#endif
