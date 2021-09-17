/*
    SPDX-FileCopyrightText: 2005 Jason Harris <kstars@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "solarsystemlistcomponent.h"

#include "kstarsdata.h"
#include "Options.h"
#ifndef KSTARS_LITE
#include "skymap.h"
#endif
#include "solarsystemcomposite.h"
#include "skyobjects/ksplanet.h"
#include "skyobjects/ksplanetbase.h"

#include <KLocalizedString>

#include <QPen>

SolarSystemListComponent::SolarSystemListComponent(SolarSystemComposite *p) : ListComponent(p), m_Earth(p->earth())
{
}

SolarSystemListComponent::~SolarSystemListComponent()
{
    //Object deletes handled by parent class (ListComponent)
}

void SolarSystemListComponent::update(KSNumbers *)
{
    if (selected())
    {
        KStarsData *data = KStarsData::Instance();

        foreach (SkyObject *o, m_ObjectList)
        {
            KSPlanetBase *p = dynamic_cast<KSPlanetBase*>(o);

            if (p)
                p->EquatorialToHorizontal(data->lst(), data->geo()->lat());
        }
    }
}

void SolarSystemListComponent::updateSolarSystemBodies(KSNumbers *num)
{
    if (selected())
    {
        KStarsData *data = KStarsData::Instance();
        foreach (SkyObject *o, m_ObjectList)
        {
            KSPlanetBase *p = (KSPlanetBase *)o;
            p->findPosition(num, data->geo()->lat(), data->lst(), m_Earth);
            p->EquatorialToHorizontal(data->lst(), data->geo()->lat());

            if (p->hasTrail())
                p->updateTrail(data->lst(), data->geo()->lat());
        }
    }
}

void SolarSystemListComponent::drawTrails(SkyPainter *skyp)
{
    //FIXME: here for all objects trails are drawn this could be source of inefficiency
    if (selected())
        foreach (SkyObject *obj, m_ObjectList)
            // Will segfault if not TrailObject
            dynamic_cast<TrailObject *>(obj)->drawTrail(skyp);
}
