/*
    SPDX-FileCopyrightText: 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "coordinategrid.h"

/**
 * @class LocalMeridianComponent
 * Single local meridian line
 *
 * @author Jasem Mutlaq
 * @version 0.1
 */
class LocalMeridianComponent : public CoordinateGrid
{
  public:
    /**
     * @short Constructor
     * Simply adds all of the coordinate grid circles (meridians and parallels)
     * @p parent Pointer to the parent SkyComposite object
     */
    explicit LocalMeridianComponent(SkyComposite *parent);

    void preDraw(SkyPainter *skyp) override;

    void update(KSNumbers *) override;

    bool selected() override;
};
