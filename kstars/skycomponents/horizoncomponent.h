/*
    SPDX-FileCopyrightText: 2005 Thomas Kabelmann <thomas.kabelmann@gmx.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "pointlistcomponent.h"

class SkyComposite;
class SkyMap;
class KSNumbers;

/**
 * @class HorizonComponent
 *
 * Represents the horizon on the sky map.
 *
 * @author Thomas Kabelmann
 * @version 0.1
 */
class HorizonComponent : public PointListComponent
{
  public:
    /**
     * @short Constructor
     *
     * @p parent Pointer to the parent SkyComposite object
     */
    explicit HorizonComponent(SkyComposite *parent);

    virtual ~HorizonComponent() override = default;

    /**
     * @short Draw the Horizon on the Sky map
     *
     * @p map Pointer to the SkyMap object
     * @p psky Reference to the QPainter on which to paint
     */
    void draw(SkyPainter *skyp) override;

    void update(KSNumbers *) override;

    bool selected() override;

  private:
    void drawCompassLabels();
};
