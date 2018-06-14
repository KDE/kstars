/*  HiPS : Hierarchical Progressive Surveys
    HiPS is the hierarchical tiling mechanism which allows one to access, visualize and browse seamlessly image, catalogue and cube data.

    The KStars HiPS compoenent is used to load and overlay progress surveys from various online catalogs.

    Copyright (C) 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#include "hipscomponent.h"

#include "Options.h"
#include "skypainter.h"
#include "skymap.h"

HIPSComponent::HIPSComponent(SkyComposite *parent) : SkyComponent(parent)
{
}

bool HIPSComponent::selected()
{
    return Options::showHIPS();
}

void HIPSComponent::draw(SkyPainter *skyp)
{
#if !defined(KSTARS_LITE)
    if (SkyMap::IsSlewing() == false && selected())
        skyp->drawHips();
#else
    Q_UNUSED(skyp);
#endif
}
