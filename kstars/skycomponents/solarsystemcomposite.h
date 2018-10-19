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

#pragma once

#include "planetmoonscomponent.h"
#include "skycomposite.h"

class AsteroidsComponent;
class CometsComponent;
class KSMoon;
class KSPlanet;
class KSSun;
//class JupiterMoonsComponent;
class SkyLabeler;
class KSEarthShadow;
/**
 * @class SolarSystemComposite
 * The solar system composite manages all planets, asteroids and comets.
 * As every sub component of solar system needs the earth , the composite
 * is managing this object by itself.
 *
 * @author Thomas Kabelmann
 * @version 0.1
 */

class SolarSystemComposite : public SkyComposite
{
  public:
    explicit SolarSystemComposite(SkyComposite *parent);
    ~SolarSystemComposite() override;

    // Use this instead of `findByName`
    KSPlanet *earth() { return m_Earth; }
    KSSun *sun() { return m_Sun; }
    KSMoon *moon() { return m_Moon; }
    KSEarthShadow *earthShadow() { return m_EarthShadow; }

    const QList<SkyObject *> &asteroids() const;
    const QList<SkyObject *> &comets() const;
    const QList<SkyObject *> &planetObjects() const;
    const QList<SkyObject *> &moons() const;

    bool selected() override;

    void update(KSNumbers *num) override;

    void updateSolarSystemBodies(KSNumbers *num) override;

    void updateMoons(KSNumbers *num) override;

    void drawTrails(SkyPainter *skyp) override;

    CometsComponent *cometsComponent();

    AsteroidsComponent *asteroidsComponent();

    QList<PlanetMoonsComponent *> planetMoonsComponent() const;

    const QList<SolarSystemSingleComponent *> &planets() const;

  private:
    KSPlanet *m_Earth { nullptr };
    KSSun *m_Sun { nullptr };
    KSMoon *m_Moon { nullptr };
    KSEarthShadow *m_EarthShadow { nullptr };

    //    PlanetMoonsComponent *m_JupiterMoons;
    AsteroidsComponent *m_AsteroidsComponent;
    CometsComponent *m_CometsComponent;
    QList<SolarSystemSingleComponent *> m_planets;
    QList<SkyObject *> m_planetObjects;
    QList<SkyObject *> m_moons;
};
