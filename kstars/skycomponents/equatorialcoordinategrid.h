/*
    SPDX-FileCopyrightText: 2012 Jerome SONRIER <jsid@emor3j.fr.eu.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "coordinategrid.h"

class SkyComposite;
class SkyPainter;

/**
 * @class EquatorialCoordinateGrid
 * Collection of all the circles in the equatorial coordinate grid
 *
 * @author Jérôme SONRIER
 * @version 0.1
 */
class EquatorialCoordinateGrid : public CoordinateGrid
{
  public:
    /**
     * @short Constructor
     * Simply adds all of the equatorial coordinate grid circles (meridians and parallels)
     * @p parent Pointer to the parent SkyComposite object
     */
    explicit EquatorialCoordinateGrid(SkyComposite *parent);

    void preDraw(SkyPainter *skyp) override;

    bool selected() override;
};
