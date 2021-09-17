/*
    SPDX-FileCopyrightText: 2012 Jerome SONRIER <jsid@emor3j.fr.eu.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "coordinategrid.h"

/**
 * @class HorizontalCoordinateGrid
 * Collection of all the circles in the horizontal coordinate grid
 *
 * @author Jérôme SONRIER
 * @version 0.1
 */
class HorizontalCoordinateGrid : public CoordinateGrid
{
  public:
    /**
     * @short Constructor
     * Simply adds all of the coordinate grid circles (meridians and parallels)
     * @p parent Pointer to the parent SkyComposite object
     */
    explicit HorizontalCoordinateGrid(SkyComposite *parent);

    void preDraw(SkyPainter *skyp) override;

    void update(KSNumbers *) override;

    bool selected() override;
};
