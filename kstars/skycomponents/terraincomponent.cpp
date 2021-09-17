/*
    SPDX-FileCopyrightText: 2021 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
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
