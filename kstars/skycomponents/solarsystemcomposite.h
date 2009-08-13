/***************************************************************************
                          solarsystemcomposite.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/01/09
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

#ifndef SOLARSYSTEMCOMPOSITE_H
#define SOLARSYSTEMCOMPOSITE_H

#include "skycomposite.h"
#include "planetmoonscomponent.h"

class KSPlanet;
class KSSun;
class KSMoon;
class JupiterMoonsComponent;
class SaturnMoonsComponent;
class AsteroidsComponent;
class CometsComponent;
class SkyLabeler;

/**@class SolarSystemComposite
* The solar system composite manages all planets, asteroids and comets.
* As every sub component of solar system needs the earth , the composite
* is managing this object by itself.
*
*@author Thomas Kabelmann
*@version 0.1
*/

class SolarSystemComposite : public SkyComposite
{
public:
    SolarSystemComposite( SkyComponent *parent, KStarsData *data );
    ~SolarSystemComposite();

    KSPlanet* earth() { return m_Earth; }
    QList<SkyObject*>& asteroids();
    QList<SkyObject*>& comets();


    virtual void init(KStarsData *data);

    bool selected();

    virtual void update( KStarsData *data, KSNumbers *num );

    virtual void updatePlanets( KStarsData *data, KSNumbers *num );

    virtual void updateMoons( KStarsData *data, KSNumbers *num );

    virtual void draw( QPainter& psky );

    void drawTrails( QPainter& psky );

    void reloadAsteroids( KStarsData *data );
    void reloadComets( KStarsData *data );

private:
    KSPlanet *m_Earth;
    KSSun *m_Sun;
    KSMoon *m_Moon;
    PlanetMoonsComponent *m_JupiterMoons;
    PlanetMoonsComponent *m_SaturnMoons;
    AsteroidsComponent *m_AsteroidsComponent;
    CometsComponent *m_CometsComponent;
    SkyLabeler* m_skyLabeler;
};

#endif
