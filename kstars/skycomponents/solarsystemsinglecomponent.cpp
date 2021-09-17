/*
    SPDX-FileCopyrightText: 2005 Thomas Kabelmann <thomas.kabelmann@gmx.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "solarsystemsinglecomponent.h"
#include "solarsystemcomposite.h"
#include "skycomponent.h"
#include <KLocalizedString>

#include "dms.h"
#include "kstarsdata.h"
#include "skyobjects/starobject.h"
#include "skyobjects/ksplanetbase.h"
#include "skyobjects/ksplanet.h"
#ifdef KSTARS_LITE
#include "skymaplite.h"
#else
#include "skymap.h"
#endif

#include "Options.h"
#include "skylabeler.h"

#include "skypainter.h"
#include "projections/projector.h"

SolarSystemSingleComponent::SolarSystemSingleComponent(SolarSystemComposite *parent, KSPlanetBase *kspb,
        bool (*visibleMethod)(), bool isMoon)
    : SkyComponent(parent), visible(visibleMethod), m_isMoon(isMoon), m_Earth(parent->earth()), m_Planet(kspb)
{
    m_Planet->loadData();
    if (!m_Planet->name().isEmpty())
    {
        objectNames(m_Planet->type()).append(m_Planet->name());
        objectLists(m_Planet->type()).append(QPair<QString, const SkyObject *>(m_Planet->name(), m_Planet));
    }
    if (!m_Planet->longname().isEmpty() && m_Planet->longname() != m_Planet->name())
    {
        objectNames(m_Planet->type()).append(m_Planet->longname());
        objectLists(m_Planet->type()).append(QPair<QString, const SkyObject *>(m_Planet->longname(), m_Planet));
    }
}

SolarSystemSingleComponent::~SolarSystemSingleComponent()
{
    removeFromNames(m_Planet);
    removeFromLists(m_Planet);
    delete m_Planet;
}

bool SolarSystemSingleComponent::selected()
{
    return visible();
}

SkyObject *SolarSystemSingleComponent::findByName(const QString &name)
{
    if (QString::compare(m_Planet->name(), name, Qt::CaseInsensitive) == 0 ||
            QString::compare(m_Planet->longname(), name, Qt::CaseInsensitive) == 0 ||
            QString::compare(m_Planet->name2(), name, Qt::CaseInsensitive) == 0)
        return m_Planet;
    return nullptr;
}

SkyObject *SolarSystemSingleComponent::objectNearest(SkyPoint *p, double &maxrad)
{
    double r = m_Planet->angularDistanceTo(p).Degrees();
    if (r < maxrad)
    {
        maxrad = r;
        return m_Planet;
    }
    return nullptr;
}

void SolarSystemSingleComponent::update(KSNumbers *)
{
    KStarsData *data = KStarsData::Instance();
    if (selected())
        m_Planet->EquatorialToHorizontal(data->lst(), data->geo()->lat());
}

void SolarSystemSingleComponent::updateSolarSystemBodies(KSNumbers *num)
{
    if (!m_isMoon && selected())
    {
        KStarsData *data = KStarsData::Instance();
        m_Planet->findPosition(num, data->geo()->lat(), data->lst(), m_Earth);
        m_Planet->EquatorialToHorizontal(data->lst(), data->geo()->lat());
        if (m_Planet->hasTrail())
            m_Planet->updateTrail(data->lst(), data->geo()->lat());
    }
}

// NOTE: This seems like code duplication, and yes IT IS. But there may be some
// NOTE: changes to be made to it later on, and calling `updateSolarSystemBodies`
// NOTE: is ugly.
void SolarSystemSingleComponent::updateMoons(KSNumbers *num)
{
    KStarsData *data = KStarsData::Instance();
    m_Planet->findPosition(num, data->geo()->lat(), data->lst(), m_Earth);
    m_Planet->EquatorialToHorizontal(data->lst(), data->geo()->lat());
    if (m_Planet->hasTrail())
        m_Planet->updateTrail(data->lst(), data->geo()->lat());
}

void SolarSystemSingleComponent::draw(SkyPainter *skyp)
{
    if (!selected())
        return;

    skyp->setPen(m_Planet->color());
    skyp->setBrush(m_Planet->color());

    bool drawn = skyp->drawPlanet(m_Planet);
    if (drawn && Options::showPlanetNames())
        SkyLabeler::AddLabel(m_Planet, SkyLabeler::PLANET_LABEL);
}

void SolarSystemSingleComponent::drawTrails(SkyPainter *skyp)
{
    if (selected())
        m_Planet->drawTrail(skyp);
}
