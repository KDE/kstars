/***************************************************************************
                      equatorialcoordinategrid.h  -  K Desktop Planetarium
                             -------------------
    begin                : Tue 01 Mar 2012
    copyright            : (C) 2012 by Jerome SONRIER
    email                : jsid@emor3j.fr.eu.org
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

    void preDraw(SkyPainter *skyp) Q_DECL_OVERRIDE;

    bool selected() Q_DECL_OVERRIDE;
};
