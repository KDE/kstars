/*
    SPDX-FileCopyrightText: Vipul Kumar Singh <vipulkrsingh@gmail.com>
    SPDX-FileCopyrightText: Médéric Boquien <mboquien@free.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "skycomponent.h"
#include "skyobjects/ksplanetbase.h"

#include <memory>

class KSNumbers;
class PlanetMoons;
class SkyComposite;
class SolarSystemSingleComponent;

/**
 * @class PlanetMoonsComponent
 * Represents the planetmoons on the sky map.
 *
 * @author Vipul Kumar Singh
 * @author Médéric boquien
 * @version 0.1
 */
class PlanetMoonsComponent : public SkyComponent
{
  public:
    /**
     * @short Constructor
     *
     * @p parent pointer to the parent SkyComposite
     */
    PlanetMoonsComponent(SkyComposite *parent, SolarSystemSingleComponent *pla, KSPlanetBase::Planets& planet);

    virtual ~PlanetMoonsComponent() override = default;

    bool selected() override;
    void draw(SkyPainter *skyp) override;
#ifndef KSTARS_LITE
    void update(KSNumbers *num) override;
#endif
    void updateMoons(KSNumbers *num) override;

    SkyObject *objectNearest(SkyPoint *p, double &maxrad) override;

    /**
     * @return a pointer to a moon if its name matches the argument
     *
     * @p name the name to be matched
     * @return a SkyObject pointer to the moon whose name matches
     * the argument, or a nullptr pointer if no match was found.
     */
    SkyObject *findByName(const QString &name) override;

    /** Return pointer to stored planet object. */
    KSPlanetBase *getPlanet() const;

    /** Return pointer to stored moons object. */
    inline PlanetMoons *getMoons() const { return pmoons.get(); }

  protected:
    void drawTrails(SkyPainter *skyp) override;

  private:
    KSPlanetBase::Planets planet;
    std::unique_ptr<PlanetMoons> pmoons;
    SolarSystemSingleComponent *m_Planet { nullptr };
};
