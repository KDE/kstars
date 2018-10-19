/***************************************************************************
                 ksearthshadow.cpp  -  K Desktop Planetarium
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

#include "earthshadowcomponent.h"
#include "skycomposite.h"
#include "skypainter.h"
#include "kstarsdata.h"

//TODO: (Valentin) Error Handling
EarthShadowComponent::EarthShadowComponent(SkyComposite *parent, KSEarthShadow *shadow) : SkyComponent(parent)
{
    m_shadow = shadow;
    m_up_to_date = false;
}

void EarthShadowComponent::update(KSNumbers *)
{
    KStarsData *data = KStarsData::Instance();
    m_shadow->EquatorialToHorizontal(data->lst(), data->geo()->lat());
}

void EarthShadowComponent::updateSolarSystemBodies(KSNumbers *num)
{
    // don't bother if the moon is not in position
    if(!m_shadow->shouldUpdate()) { // TODO: There must be sth. more efficient
        m_up_to_date = false;
        return;
    }

    KStarsData *data = KStarsData::Instance();
    CachingDms *LST = data->lst();
    const CachingDms *lat = data->geo()->lat();

    m_shadow->findPosition(num, lat, LST);
    m_shadow->EquatorialToHorizontal(LST, lat);
    m_up_to_date = true;
}

void EarthShadowComponent::draw(SkyPainter *skyp)
{
    // check if the shadow has been updated
    if(m_up_to_date && (m_shadow->isInEclipse()))
        skyp->drawEarthShadow(m_shadow);
}
