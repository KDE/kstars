/*  HiPS : Hierarchical Progressive Surveys
    HiPS is the hierarchical tiling mechanism which allows one to access, visualize and browse seamlessly image, catalogue and cube data.

    The KStars HiPS compoenent is used to load and overlay progress surveys from various online catalogs.

    SPDX-FileCopyrightText: 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
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
  if (((SkyMap::IsSlewing() == false) || Options::hIPSPanning()) && selected())
        skyp->drawHips();
#else
    Q_UNUSED(skyp);
#endif
}
