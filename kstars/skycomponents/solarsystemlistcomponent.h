/***************************************************************************
                solarsystemlistcomponent.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/22/09
    copyright            : (C) 2005 by Jason Harris
    email                : kstars@30doradus.org
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
