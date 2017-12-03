/*  Local Meridian Component
    Copyright (C) 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
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
