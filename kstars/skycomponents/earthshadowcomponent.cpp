/*
    SPDX-FileCopyrightText: 2018 Valentin Boettcher <valentin@boettcher.cf (do not hesitate to contact)>
    matrix               : @hiro98@tchncs.de

    SPDX-License-Identifier: GPL-2.0-or-later
*/

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
