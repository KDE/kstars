/*
    SPDX-FileCopyrightText: 2018 Valentin Boettcher <valentin@boettcher.cf (do not hesitate to contact)>
    matrix               : @hiro98@tchncs.de

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once
#include "skycomponent.h"
#include "ksearthshadow.h" // in here for inline definitions

class SkyComposite;
class SkyPainter;

/**
 * @brief The EarthShadowComponent class
 * @short A simple skycomponent for the KSEarthShadow.
 */
class EarthShadowComponent : public SkyComponent
{
public:
    EarthShadowComponent(SkyComposite * parent, KSEarthShadow * shadow);

    void update(KSNumbers *num) override;
    void updateSolarSystemBodies(KSNumbers *num) override;
    bool selected() override { return m_shadow->shouldUpdate(); }
    void draw(SkyPainter *skyp) override;

private:
    KSEarthShadow * m_shadow;
    bool m_up_to_date;
};
