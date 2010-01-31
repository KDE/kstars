/***************************************************************************
                          solarsystemsinglecomponent.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/30/08
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

#ifndef SOLARSYSTEMSINGLECOMPONENT_H
#define SOLARSYSTEMSINGLECOMPONENT_H

#include <QColor>

/**
	*@class SolarSystemSingleComponent
	*This class encapsulates some methods which are shared between
	*all single-object solar system components (Sun, Moon, Planet, Pluto)
	*
	*@author Thomas Kabelmann
	*@version 0.1
	*/

#include "skycomponent.h"

class SolarSystemComposite;
class KSNumbers;
class KSPlanet;
class KSPlanetBase;
class SkyLabeler;

class SolarSystemSingleComponent : public SkyComponent
{
public:
    /** Initialize visible method, minimum size and sizeScale. */
    SolarSystemSingleComponent(SolarSystemComposite*, KSPlanetBase *kspb, bool (*visibleMethod)());

    virtual ~SolarSystemSingleComponent();

    /** Return pointer to stored planet object. */
    KSPlanetBase* planet() { return m_Planet; }

    virtual void init();
    virtual void update( KSNumbers *num );
    virtual void updatePlanets( KSNumbers *num );
    virtual SkyObject* first();
    virtual SkyObject* next();
    virtual SkyObject* findByName( const QString &name );
    virtual SkyObject* objectNearest( SkyPoint *p, double &maxrad );
    virtual void draw( QPainter &psky );
protected:
    /**@short Draws the planet's trail, if necessary. */
    virtual void drawTrails( QPainter& psky );

    /**@short Add a Trail to the specified SkyObject.
     * @p o Pointer to the SkyObject to which a Trail will be added */
    virtual bool addTrail( SkyObject *o );

    /**@return true if the specified SkyObject is a member of this component, and it contains a Trail.
     * @p o Pointer to the SkyObject to which a Trail will be added
     */
    virtual bool hasTrail( SkyObject *o, bool& found );
    virtual bool removeTrail( SkyObject *o );
    virtual void clearTrailsExcept( SkyObject *exOb );

private:
    QColor        m_Color;
    KSPlanet     *m_Earth;
    KSPlanetBase *m_Planet;
};

#endif
