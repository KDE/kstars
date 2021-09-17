/*
    SPDX-FileCopyrightText: 2005 Thomas Kabelmann <thomas.kabelmann@gmx.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

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
