/*  The terrain component is used to draw the terrain.

    Copyright (C) 2021 Hy Murveit <hy@murveit.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#include "terraincomponent.h"

#include "Options.h"
#include "skypainter.h"
#include "skymap.h"

TerrainComponent::TerrainComponent(SkyComposite *parent) : SkyComponent(parent)
{
}

bool TerrainComponent::selected()
{
    return Options::showTerrain();
}

void TerrainComponent::draw(SkyPainter *skyp)
{
#if !defined(KSTARS_LITE)
    if (((SkyMap::IsSlewing() == false) || Options::terrainPanning()) && selected())
        skyp->drawTerrain();
#else
    Q_UNUSED(skyp);
#endif
}
