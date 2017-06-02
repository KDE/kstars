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
//class JupiterMoonsComponent;
class AsteroidsComponent;
class CometsComponent;
class SkyLabeler;

/** @class SolarSystemComposite
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
        explicit SolarSystemComposite( SkyComposite * parent );
        ~SolarSystemComposite();

        KSPlanet * earth()
        {
            return m_Earth;
        }

        KSSun * sun()
        {
            return m_Sun;
        }

        const QList<SkyObject *> &asteroids() const;
        const QList<SkyObject *> &comets() const;
        const QList<SkyObject *> &planetObjects() const;
        const QList<SkyObject *> &moons() const;

        bool selected() Q_DECL_OVERRIDE;

        void update( KSNumbers * num ) Q_DECL_OVERRIDE;

        void updateSolarSystemBodies( KSNumbers * num ) Q_DECL_OVERRIDE;

        void updateMoons( KSNumbers * num ) Q_DECL_OVERRIDE;

        void drawTrails( SkyPainter * skyp ) Q_DECL_OVERRIDE;

        CometsComponent * cometsComponent();

        AsteroidsComponent * asteroidsComponent();

        QList<PlanetMoonsComponent *> planetMoonsComponent() const;

        const QList<SolarSystemSingleComponent *> &planets() const;
    private:
        KSPlanet * m_Earth;
        KSSun * m_Sun;
        KSMoon * m_Moon;
//    PlanetMoonsComponent *m_JupiterMoons;
        AsteroidsComponent * m_AsteroidsComponent;
        CometsComponent * m_CometsComponent;
        QList<SolarSystemSingleComponent *> m_planets;
        QList<SkyObject *> m_planetObjects;
        QList<SkyObject *> m_moons;
};

#endif
