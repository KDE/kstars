/***************************************************************************
                    ksearthshadow.h  -  K Desktop Planetarium
                             -------------------
    begin                : Fri Aug 24 2018
    copyright            : (C) 2018 by Valentin Boettcher
    email                : valentin@boettcher.cf (do not hesitate to contact)
    matrix               : @hiro98@tchncs.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

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
