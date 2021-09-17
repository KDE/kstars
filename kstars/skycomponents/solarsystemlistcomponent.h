/*
    SPDX-FileCopyrightText: 2005 Jason Harris <kstars@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "listcomponent.h"

class KSPlanet;
class SolarSystemComposite;

/**
 * @class SolarSystemListComponent
 *
 * @author Jason Harris
 * @version 1.0
 */
class SolarSystemListComponent : public ListComponent
{
  public:
    explicit SolarSystemListComponent(SolarSystemComposite *parent);

    ~SolarSystemListComponent() override;

    void update(KSNumbers *num) override;

    /**
     * @short Update the coordinates of the solar system bodies in this component.
     *
     * This function updates the position of the moving solar system bodies.
     * @p data Pointer to the KStarsData object
     * @p num Pointer to the KSNumbers object
     */
    void updateSolarSystemBodies(KSNumbers *num) override;

  protected:
    void drawTrails(SkyPainter *skyp) override;

  private:
    KSPlanet *m_Earth { nullptr };
};
