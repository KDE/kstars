/*  The terrain component is used to draw the terrain.

    Copyright (C) 2021 Hy Murveit <hy@murveit.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#pragma once

#include "terraincomponent.h"
#include "skycomponent.h"

/**
 * @class TerrainComponent
 * Represents the terrain overlay
 * @author Hy Murveit
 * @version 1.0
 */
class TerrainComponent : public SkyComponent
{
  public:
    /** Constructor */
    explicit TerrainComponent(SkyComposite *);

    virtual ~TerrainComponent() override = default;

    bool selected() override;
    void draw(SkyPainter *skyp) override;
};
