/*  HiPS : Hierarchical Progressive Surveys
    HiPS is the hierarchical tiling mechanism which allows one to access, visualize and browse seamlessly image, catalogue and cube data.

    The KStars HiPS compoenent is used to load and overlay progress surverys from various online catalogs.

    Copyright (C) 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#pragma once

#include "hipscomponent.h"
#include "skycomponent.h"

/**
 * @class HIPSComponent
 * Represents the HIPS progress survey overlay
 * @author Jasem Mutlaq
 * @version 1.0
 */
class HIPSComponent : public SkyComponent
{
  public:
    /** Constructor */
    explicit HIPSComponent(SkyComposite *);

    /** Destructor */
    ~HIPSComponent();

    void draw(SkyPainter *skyp) override;
};
