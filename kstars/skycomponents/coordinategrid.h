/*
    SPDX-FileCopyrightText: 2005 Jason Harris <kstars@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "noprecessindex.h"

class QString;

class SkyComposite;
class SkyPainter;

/**
 * @class CoordinateGrid
 * Collection of all the circles in the coordinate grid
 *
 * @author Jason Harris
 * @version 0.1
 */
class CoordinateGrid : public NoPrecessIndex
{
  public:
    /**
     * @short Constructor
     * Simply adds all of the coordinate grid circles (meridians and parallels)
     * @p parent Pointer to the parent SkyComposite object
     */
    CoordinateGrid(SkyComposite *parent, const QString &name);

    void preDraw(SkyPainter *skyp) override = 0;

    bool selected() override = 0;
};
