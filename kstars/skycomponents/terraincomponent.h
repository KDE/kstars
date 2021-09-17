/*
    SPDX-FileCopyrightText: 2021 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
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
