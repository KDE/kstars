/*
    SPDX-FileCopyrightText: 2009 Vipul Kumar Singh Médéric Boquien <vipulkrsingh@gmail.com, mboquien@free.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "planetmoonscomponent.h"

#include "kstarsdata.h"
#include "Options.h"
#include "skylabeler.h"
#include "skypainter.h"
#include "kssun.h"
#include "solarsystemcomposite.h"
#include "solarsystemsinglecomponent.h"
#include "projections/projector.h"
#include "skyobjects/jupitermoons.h"

PlanetMoonsComponent::PlanetMoonsComponent(SkyComposite *p, SolarSystemSingleComponent *planetComponent,
        KSPlanetBase::Planets &_planet)
    : SkyComponent(p), planet(_planet), m_Planet(planetComponent)
{
    /*
    if (planet == KSPlanetBase::JUPITER)
        pmoons = new JupiterMoons();
    else
        pmoons = new SaturnMoons();
    */
    Q_ASSERT(planet == KSPlanetBase::JUPITER);
    //    delete pmoons;
    //    pmoons = new JupiterMoons();
    //    int nmoons = pmoons->nMoons();

    //    for (int i = 0; i < nmoons; ++i)
    //    {
    //        objectNames(SkyObject::MOON).append( pmoons->name(i) );
    //        objectLists(SkyObject::MOON).append( QPair<QString, const SkyObject*>(pmoons->name(i),pmoons->moon(i)) );
    //    }
}

bool PlanetMoonsComponent::selected()
{
    return m_Planet->selected();
}

#ifndef KSTARS_LITE
void PlanetMoonsComponent::update(KSNumbers *)
{
    KStarsData *data = KStarsData::Instance();
    if (selected())
        pmoons->EquatorialToHorizontal(data->lst(), data->geo()->lat());
}
#endif

void PlanetMoonsComponent::updateMoons(KSNumbers *num)
{
    //FIXME: evil cast
    if (selected())
        pmoons->findPosition(num, m_Planet->planet(), dynamic_cast<KSSun *>(parent()->findByName(i18n("Sun"))));
}

SkyObject *PlanetMoonsComponent::findByName(const QString &name)
{
    int nmoons = pmoons->nMoons();

    for (int i = 0; i < nmoons; ++i)
    {
        TrailObject *moon = pmoons->moon(i);
        if (QString::compare(moon->name(), name, Qt::CaseInsensitive) == 0 ||
                QString::compare(moon->longname(), name, Qt::CaseInsensitive) == 0 ||
                QString::compare(moon->name2(), name, Qt::CaseInsensitive) == 0)
            return moon;
    }

    return nullptr;
}

#ifdef KSTARS_LITE
KSPlanetBase *PlanetMoonsComponent::getPlanet() const
{
    return m_Planet->planet();
}
#endif

SkyObject *PlanetMoonsComponent::objectNearest(SkyPoint *p, double &maxrad)
{
    SkyObject *oBest = nullptr;
    int nmoons       = pmoons->nMoons();

    if (Options::zoomFactor() < 3000)
        return nullptr;

    for (int i = 0; i < nmoons; ++i)
    {
        SkyObject *moon = pmoons->moon(i);
        double r        = moon->angularDistanceTo(p).Degrees();
        if (r < maxrad)
        {
            maxrad = r;
            oBest  = moon;
        }
    }
    return oBest;
}

void PlanetMoonsComponent::draw(SkyPainter *skyp)
{
    if (!(planet == KSPlanetBase::JUPITER && Options::showJupiter()))
        return;

    //In order to get the z-order right for the moons and the planet,
    //we need to first draw the moons that are further away than the planet,
    //then re-draw the planet, then draw the moons nearer than the planet.
    QList<TrailObject *> frontMoons;
    int nmoons = pmoons->nMoons();

    for (int i = 0; i < nmoons; ++i)
    {
        if (pmoons->z(i) < 0.0) //Moon is nearer than the planet
        {
            frontMoons.append(pmoons->moon(i));
        }
        else
        {
            //Draw Moons that are further than the planet
            skyp->drawPointSource(pmoons->moon(i), pmoons->moon(i)->mag());
        }
    }

    //Now redraw the planet
    m_Planet->draw(skyp);

    //Now draw the remaining moons, as stored in frontMoons
    foreach (TrailObject *moon, frontMoons)
    {
        skyp->drawPointSource(moon, moon->mag());
    }

    //Draw Moon name labels if at high zoom
    if (!(Options::showPlanetNames() && Options::zoomFactor() > 50. * MINZOOM))
        return;
    for (int i = 0; i < nmoons; ++i)
    {
        /*
        if (planet ==KSPlanetBase::SATURN)
            SkyLabeler::AddLabel( pmoons->moon(i), SkyLabeler::SATURN_MOON_LABEL );
        else
        */
        SkyLabeler::AddLabel(pmoons->moon(i), SkyLabeler::JUPITER_MOON_LABEL);
    }
}

void PlanetMoonsComponent::drawTrails(SkyPainter *skyp)
{
    if (!selected())
        return;
    int nmoons = pmoons->nMoons();
    for (int i = 0; i < nmoons; ++i)
        pmoons->moon(i)->drawTrail(skyp);
}
