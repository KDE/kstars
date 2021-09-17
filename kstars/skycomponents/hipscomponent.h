/*  HiPS : Hierarchical Progressive Surveys
    HiPS is the hierarchical tiling mechanism which allows one to access, visualize and browse seamlessly image, catalogue and cube data.

    The KStars HiPS compoenent is used to load and overlay progress surverys from various online catalogs.

    SPDX-FileCopyrightText: 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
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

    virtual ~HIPSComponent() override = default;

    bool selected() override;
    void draw(SkyPainter *skyp) override;
};
