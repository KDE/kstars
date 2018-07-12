/***************************************************************************
                          skymapcomposite.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/07/08
    copyright            : (C) 2005 by Thomas Kabelmann
    email                : thomas.kabelmann@gmx.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "skymapcomposite.h"

#include "artificialhorizoncomponent.h"
#include "catalogcomponent.h"
#include "constellationartcomponent.h"
#include "constellationboundarylines.h"
#include "constellationlines.h"
#include "constellationnamescomponent.h"
#include "culturelist.h"
#include "deepskycomponent.h"
#include "deepstarcomponent.h"
#include "ecliptic.h"
#include "equator.h"
#include "equatorialcoordinategrid.h"
#include "horizoncomponent.h"
#include "horizontalcoordinategrid.h"
#include "localmeridiancomponent.h"
#include "ksasteroid.h"
#include "kscomet.h"
#ifndef KSTARS_LITE
#include "kstars.h"
#endif
#include "kstarsdata.h"
#include "milkyway.h"
#include "satellitescomponent.h"
#include "skylabeler.h"
#include "skypainter.h"
#include "solarsystemcomposite.h"
#include "starcomponent.h"
#include "supernovaecomponent.h"
#include "syncedcatalogcomponent.h"
#include "targetlistcomponent.h"
#include "projections/projector.h"
#include "skyobjects/deepskyobject.h"
#include "skyobjects/ksplanet.h"
#include "skyobjects/constellationsart.h"

#ifndef KSTARS_LITE
#include "flagcomponent.h"
#include "ksutils.h"
#include "observinglist.h"
#include "skymap.h"
#include "hipscomponent.h"
#endif

#include <QApplication>

#include <kstars_debug.h>

SkyMapComposite::SkyMapComposite(SkyComposite *parent) : SkyComposite(parent), m_reindexNum(J2000)
{
    m_skyLabeler.reset(SkyLabeler::Instance());
    m_skyMesh.reset(SkyMesh::Create(3)); // level 5 mesh = 8192 trixels
    m_skyMesh->debug(0);
//  1 => print "indexing ..."
//  2 => prints totals too
// 10 => prints detailed lists
// You can also set the debug level of individual
// appendLine() and appendPoly() calls.

//Add all components
//Stars must come before constellation lines
#ifdef KSTARS_LITE
    addComponent(m_MilkyWay = new MilkyWay(this), 50);
    addComponent(m_Stars = StarComponent::Create(this), 10);
    addComponent(m_EquatorialCoordinateGrid = new EquatorialCoordinateGrid(this));
    addComponent(m_HorizontalCoordinateGrid = new HorizontalCoordinateGrid(this));

    // Do add to components.
    addComponent(m_CBoundLines = new ConstellationBoundaryLines(this), 80);
    m_Cultures.reset(new CultureList());
    addComponent(m_CLines = new ConstellationLines(this, m_Cultures.get()), 85);
    addComponent(m_CNames = new ConstellationNamesComponent(this, m_Cultures.get()), 90);
    addComponent(m_Equator = new Equator(this), 95);
    addComponent(m_Ecliptic = new Ecliptic(this), 95);
    addComponent(m_Horizon = new HorizonComponent(this), 100);
    addComponent(m_DeepSky = new DeepSkyComponent(this), 5);
    addComponent(m_ConstellationArt = new ConstellationArtComponent(this, m_Cultures.get()), 100);

    addComponent(m_ArtificialHorizon = new ArtificialHorizonComponent(this), 110);

    m_internetResolvedCat = "_Internet_Resolved";
    m_manualAdditionsCat  = "_Manual_Additions";
    addComponent(m_internetResolvedComponent = new SyncedCatalogComponent(this, m_internetResolvedCat, true, 0), 6);
    addComponent(m_manualAdditionsComponent = new SyncedCatalogComponent(this, m_manualAdditionsCat, true, 0), 6);
    m_CustomCatalogs.reset(new SkyComposite(this));
    QStringList allcatalogs = Options::showCatalogNames();

    if (!allcatalogs.contains(m_internetResolvedCat))
    {
        allcatalogs.append(m_internetResolvedCat);
    }
    if (!allcatalogs.contains(m_manualAdditionsCat))
    {
        allcatalogs.append(m_manualAdditionsCat);
    }
    Options::setShowCatalogNames(allcatalogs);

    for (int i = 0; i < allcatalogs.size(); ++i)
    {
        if (allcatalogs.at(i) == m_internetResolvedCat ||
            allcatalogs.at(i) == m_manualAdditionsCat) // This is a special catalog
            continue;
        m_CustomCatalogs->addComponent(new CatalogComponent(this, allcatalogs.at(i), false, i),
                                       6); // FIXME: Should this be 6 or 5? See SkyMapComposite::reloadDeepSky()
    }

    addComponent(m_SolarSystem = new SolarSystemComposite(this), 2);

    //addComponent( m_ObservingList = new TargetListComponent( this , 0, QPen(),
    //                                                       &Options::obsListSymbol, &Options::obsListText ), 120 );
    addComponent(m_StarHopRouteList = new TargetListComponent(this, 0, QPen()), 130);
    addComponent(m_Satellites = new SatellitesComponent(this), 7);
    addComponent(m_Supernovae = new SupernovaeComponent(this), 7);
    SkyMapLite::Instance()->loadingFinished();
#else
    addComponent(m_MilkyWay = new MilkyWay(this), 50);
    addComponent(m_Stars = StarComponent::Create(this), 10);
    addComponent(m_EquatorialCoordinateGrid = new EquatorialCoordinateGrid(this));
    addComponent(m_HorizontalCoordinateGrid = new HorizontalCoordinateGrid(this));
    addComponent(m_LocalMeridianComponent = new LocalMeridianComponent(this));

    // Do add to components.
    addComponent(m_CBoundLines = new ConstellationBoundaryLines(this), 80);
    m_Cultures.reset(new CultureList());
    addComponent(m_CLines = new ConstellationLines(this, m_Cultures.get()), 85);
    addComponent(m_CNames = new ConstellationNamesComponent(this, m_Cultures.get()), 90);
    addComponent(m_Equator = new Equator(this), 95);
    addComponent(m_Ecliptic = new Ecliptic(this), 95);
    addComponent(m_Horizon = new HorizonComponent(this), 100);
    addComponent(m_DeepSky = new DeepSkyComponent(this), 5);
    addComponent(m_ConstellationArt = new ConstellationArtComponent(this, m_Cultures.get()), 100);

    // Hips
    addComponent(m_HiPS = new HIPSComponent(this));

    addComponent(m_ArtificialHorizon = new ArtificialHorizonComponent(this), 110);

    m_internetResolvedCat = "_Internet_Resolved";
    m_manualAdditionsCat  = "_Manual_Additions";
    addComponent(m_internetResolvedComponent = new SyncedCatalogComponent(this, m_internetResolvedCat, true, 0), 6);
    addComponent(m_manualAdditionsComponent = new SyncedCatalogComponent(this, m_manualAdditionsCat, true, 0), 6);
    m_CustomCatalogs.reset(new SkyComposite(this));
    QStringList allcatalogs = Options::showCatalogNames();
    for (int i = 0; i < allcatalogs.size(); ++i)
    {
        if (allcatalogs.at(i) == m_internetResolvedCat ||
            allcatalogs.at(i) == m_manualAdditionsCat) // This is a special catalog
            continue;
        m_CustomCatalogs->addComponent(new CatalogComponent(this, allcatalogs.at(i), false, i),
                                       6); // FIXME: Should this be 6 or 5? See SkyMapComposite::reloadDeepSky()
    }

    addComponent(m_SolarSystem = new SolarSystemComposite(this), 2);

    addComponent(m_Flags = new FlagComponent(this), 4);       

    addComponent(m_ObservingList =
                     new TargetListComponent(this, nullptr, QPen(), &Options::obsListSymbol, &Options::obsListText),
                 120);
    addComponent(m_StarHopRouteList = new TargetListComponent(this, nullptr, QPen()), 130);
    addComponent(m_Satellites = new SatellitesComponent(this), 7);
    addComponent(m_Supernovae = new SupernovaeComponent(this), 7);
#endif
    connect(this, SIGNAL(progressText(QString)), KStarsData::Instance(), SIGNAL(progressText(QString)));
}

void SkyMapComposite::update(KSNumbers *num)
{
    //printf("updating SkyMapComposite\n");
    //1. Milky Way
    //m_MilkyWay->update( data, num );
    //2. Coordinate grid
    //m_EquatorialCoordinateGrid->update( num );
    m_HorizontalCoordinateGrid->update(num);
    #ifndef KSTARS_LITE
    m_LocalMeridianComponent->update(num);
    #endif
    //3. Constellation boundaries
    //m_CBounds->update( data, num );
    //4. Constellation lines
    //m_CLines->update( data, num );
    //5. Constellation names
    if (m_CNames)
        m_CNames->update(num);
    //6. Equator
    //m_Equator->update( data, num );
    //7. Ecliptic
    //m_Ecliptic->update( data, num );
    //8. Deep sky
    //m_DeepSky->update( data, num );
    //9. Custom catalogs
    m_CustomCatalogs->update(num);
    m_internetResolvedComponent->update(num);
    m_manualAdditionsComponent->update(num);
    //10. Stars
    //m_Stars->update( data, num );
    //m_CLines->update( data, num );  // MUST follow stars.

    //12. Solar system
    m_SolarSystem->update(num);
    //13. Satellites
    m_Satellites->update(num);
    //14. Supernovae
    m_Supernovae->update(num);
    //15. Horizon
    m_Horizon->update(num);
#ifndef KSTARS_LITE
    //16. Flags
    m_Flags->update(num);
#endif
}

void SkyMapComposite::updateSolarSystemBodies(KSNumbers *num)
{
    m_SolarSystem->updateSolarSystemBodies(num);
}

/*
void SkyMapComposite::updateMoons(KSNumbers *num )
{
    m_SolarSystem->updateMoons( num );
}
*/

//Reimplement draw function so that we have control over the order of
//elements, and we can add object labels
//
//The order in which components are drawn naturally determines the
//z-ordering (the layering) of the components.  Objects which
//should appear "behind" others should be drawn first.
void SkyMapComposite::draw(SkyPainter *skyp)
{
    Q_UNUSED(skyp)
#ifndef KSTARS_LITE
    SkyMap *map      = SkyMap::Instance();
    KStarsData *data = KStarsData::Instance();

    // We delay one draw cycle before re-indexing
    // we MUST ensure CLines do not get re-indexed while we use DRAW_BUF
    // so we do it here.
    m_CLines->reindex(&m_reindexNum);
    // This queues re-indexing for the next draw cycle
    m_reindexNum = KSNumbers(data->updateNum()->julianDay());

    // This ensures that the JIT updates are synchronized for the entire draw
    // cycle so the sky moves as a single sheet.  May not be needed.
    data->syncUpdateIDs();

    // prepare the aperture
    // FIXME_FOV: We may want to rejigger this to allow
    // wide-angle views --hdevalence
    float radius = map->projector()->fov();
    if (radius > 180.0)
        radius = 180.0;

    if (m_skyMesh->inDraw())
    {
        printf("Warning: aborting concurrent SkyMapComposite::draw()\n");
        return;
    }

    m_skyMesh->inDraw(true);
    SkyPoint *focus = map->focus();
    m_skyMesh->aperture(focus, radius + 1.0, DRAW_BUF); // divide by 2 for testing

    // create the no-precess aperture if needed
    if (Options::showEquatorialGrid() || Options::showHorizontalGrid() || Options::showCBounds() ||
        Options::showEquator())
    {
        m_skyMesh->index(focus, radius + 1.0, NO_PRECESS_BUF);
    }

    // clear marks from old labels and prep fonts
    m_skyLabeler->reset(map);
    m_skyLabeler->useStdFont();

    // info boxes have highest label priority
    // FIXME: REGRESSION. Labeler now know nothing about infoboxes
    // map->infoBoxes()->reserveBoxes( psky );

    // JM 2016-12-01: Why is this done this way?!! It's too inefficient
    if (KStars::Instance())
    {
        auto &obsList = KStarsData::Instance()->observingList()->sessionList();

        if (Options::obsListText())
            for (auto &obj_clone : obsList)
            {
                // Find the "original" obj
                SkyObject *o = findByName(obj_clone->name()); // FIXME: This is sloww.... and can also fail!!!
                if (!o)
                    continue;
                SkyLabeler::AddLabel(o, SkyLabeler::RUDE_LABEL);
            }
    }

    m_MilkyWay->draw(skyp);

    // Draw HIPS after milky way but before everything else
    m_HiPS->draw(skyp);

    m_EquatorialCoordinateGrid->draw(skyp);
    m_HorizontalCoordinateGrid->draw(skyp);
    m_LocalMeridianComponent->draw(skyp);

    //Draw constellation boundary lines only if we draw western constellations
    if (m_Cultures->current() == "Western")
    {
        m_CBoundLines->draw(skyp);
        m_ConstellationArt->draw(skyp);
    }
    else if (m_Cultures->current() == "Inuit")
    {
        m_ConstellationArt->draw(skyp);
    }

    m_CLines->draw(skyp);

    m_Equator->draw(skyp);

    m_Ecliptic->draw(skyp);

    m_DeepSky->draw(skyp);

    m_CustomCatalogs->draw(skyp);
    m_internetResolvedComponent->draw(skyp);
    m_manualAdditionsComponent->draw(skyp);

    m_Stars->draw(skyp);

    m_SolarSystem->drawTrails(skyp);
    m_SolarSystem->draw(skyp);

    m_Satellites->draw(skyp);

    m_Supernovae->draw(skyp);

    map->drawObjectLabels(labelObjects());

    m_skyLabeler->drawQueuedLabels();
    m_CNames->draw(skyp);
    m_Stars->drawLabels();
    m_DeepSky->drawLabels();

    m_ObservingList->pen = QPen(QColor(data->colorScheme()->colorNamed("ObsListColor")), 1.);
    m_ObservingList->list2 = KStarsData::Instance()->observingList()->sessionList();
    m_ObservingList->draw(skyp);

    m_Flags->draw(skyp);

    m_StarHopRouteList->pen = QPen(QColor(data->colorScheme()->colorNamed("StarHopRouteColor")), 1.);
    m_StarHopRouteList->draw(skyp);

    m_ArtificialHorizon->draw(skyp);

    m_Horizon->draw(skyp);

    m_skyMesh->inDraw(false);

// DEBUG Edit. Keywords: Trixel boundaries. Currently works only in QPainter mode
// -jbb uncomment these to see trixel outlines:
/*
    QPainter *psky = dynamic_cast< QPainter *>( skyp );
    if( psky ) {
        qCDebug(KSTARS) << "Drawing trixel boundaries for debugging.";
        psky->setPen(  QPen( QBrush( QColor( "yellow" ) ), 1, Qt::SolidLine ) );
        m_skyMesh->draw( *psky, OBJ_NEAREST_BUF );
        SkyMesh *p;
        if( p = SkyMesh::Instance( 6 ) ) {
            qCDebug(KSTARS) << "We have a deep sky mesh to draw";
            p->draw( *psky, OBJ_NEAREST_BUF );
        }

        psky->setPen( QPen( QBrush( QColor( "green" ) ), 1, Qt::SolidLine ) );
        m_skyMesh->draw( *psky, NO_PRECESS_BUF );
        if( p )
            p->draw( *psky, NO_PRECESS_BUF );
    }
    */
#endif
}

//Select nearest object to the given skypoint, but give preference
//to certain object types.
//we multiply each object type's smallest angular distance by the
//following factors before selecting the final nearest object:
// faint stars > 12th mag = 1.75
// stars > 4 and < 12 = 1.5
// stars brighter than 4th mag = 0.75
// IC catalog = 0.8
// NGC catalog = 0.6
// "other" catalog = 0.6
// Messier object = 0.5
// custom object = 0.5
// Solar system = 0.25
SkyObject *SkyMapComposite::objectNearest(SkyPoint *p, double &maxrad)
{
    double rTry      = maxrad;
    double rBest     = maxrad;
    SkyObject *oTry  = nullptr;
    SkyObject *oBest = nullptr;

    //printf("%.1f %.1f\n", p->ra().Degrees(), p->dec().Degrees() );
    m_skyMesh->aperture(p, maxrad + 1.0, OBJ_NEAREST_BUF);

    oBest = m_Stars->objectNearest(p, rBest);
    //reduce rBest by 0.75 for stars brighter than 4th mag
    if (oBest && oBest->mag() < 4.0)
        rBest *= 0.75;
    // For stars fainter than 12th mag
    else if (oBest && oBest->mag() > 12.0)
        rBest *= 1.75;
    // For stars between 4th and 12th mag
    else if (oBest)
        rBest *= 1.5;

    oTry = m_Satellites->objectNearest(p, rTry);
    if (rTry < rBest)
    {
        rBest = rTry;
        oBest = oTry;
    }    

    for (auto &star : m_DeepStars)
    {
        rTry = maxrad;
        oTry = star->objectNearest(p, rTry);
        if (rTry < rBest)
        {
            rBest = rTry;
            oBest = oTry;
        }
    }

    // TODO: Add support for deep star catalogs

    //m_DeepSky internally discriminates among deepsky catalogs
    //and renormalizes rTry
    oTry = m_DeepSky->objectNearest(p, rTry);
    if (oTry && rTry < rBest)
    {
        rBest = rTry;
        oBest = oTry;
    }

    rTry = maxrad;
    oTry = m_CustomCatalogs->objectNearest(p, rTry);
    rTry *= 0.5;
    if (oTry && rTry < rBest)
    {
        rBest = rTry;
        oBest = oTry;
    }

    rTry = maxrad;
    oTry = m_internetResolvedComponent->objectNearest(p, rTry);
    rTry *= 0.5;
    if (oTry && rTry < rBest)
    {
        rBest = rTry;
        oBest = oTry;
    }

    rTry = maxrad;
    oTry = m_manualAdditionsComponent->objectNearest(p, rTry);
    rTry *= 0.5;
    if (oTry && rTry < rBest)
    {
        rBest = rTry;
        oBest = oTry;
    }

    rTry = maxrad;
    oTry = m_Supernovae->objectNearest(p, rTry);
    //qCDebug(KSTARS)<<rTry<<rBest<<maxrad;
    if (rTry < rBest)
    {
        rBest = rTry;
        oBest = oTry;
    }

    rTry = maxrad;
    oTry = m_SolarSystem->objectNearest(p, rTry);
    if (!dynamic_cast<KSComet *>(oTry) &&
        !dynamic_cast<KSAsteroid *>(
            oTry)) // There are gazillions of faint asteroids and comets; we want to prevent them from getting precedence
    {
        rTry *= 0.25; // this is either sun, moon, or one of the major planets or their moons.
    }
    else
    {
        if (std::isfinite(oTry->mag()) && oTry->mag() < 12.0)
        {
            rTry *= 0.75; // Bright comets / asteroids get some precedence.
        }
    }
    if (rTry < rBest)
    {
        rBest = rTry;
        oBest = oTry;
    }

    //if ( oBest && Options::verboseLogging())
    //qCDebug(KSTARS) << "OBEST=" << oBest->name() << " - " << oBest->name2();
    maxrad = rBest;
    return oBest; //will be 0 if no object nearer than maxrad was found
}

SkyObject *SkyMapComposite::starNearest(SkyPoint *p, double &maxrad)
{
    double rtry     = maxrad;
    SkyObject *star = nullptr;

    m_skyMesh->aperture(p, maxrad + 1.0, OBJ_NEAREST_BUF);

    star = m_Stars->objectNearest(p, rtry);
    //reduce rBest by 0.75 for stars brighter than 4th mag
    if (star && star->mag() < 4.0)
        rtry *= 0.75;

    // TODO: Add Deep Star Catalog support

    maxrad = rtry;
    return star;
}

bool SkyMapComposite::addNameLabel(SkyObject *o)
{
    if (!o)
        return false;
    labelObjects().append(o);
    return true;
}

bool SkyMapComposite::removeNameLabel(SkyObject *o)
{
    if (!o)
        return false;
    int index = labelObjects().indexOf(o);
    if (index < 0)
        return false;
    labelObjects().removeAt(index);
    return true;
}

QHash<int, QStringList> &SkyMapComposite::getObjectNames()
{
    return m_ObjectNames;
}

QHash<int, QVector<QPair<QString, const SkyObject *>>> &SkyMapComposite::getObjectLists()
{
    return m_ObjectLists;
}

QList<SkyObject *> SkyMapComposite::findObjectsInArea(const SkyPoint &p1, const SkyPoint &p2)
{
    const SkyRegion &region = m_skyMesh->skyRegion(p1, p2);
    QList<SkyObject *> list;
    // call objectsInArea( QList<SkyObject*>&, const SkyRegion& ) for each of the
    // components of the SkyMapComposite
    if (m_Stars->selected())
        m_Stars->objectsInArea(list, region);
    if (m_DeepSky->selected())
        m_DeepSky->objectsInArea(list, region);
    return list;
}

SkyObject *SkyMapComposite::findByName(const QString &name)
{
#ifndef KSTARS_LITE
    if (KStars::Closing)
        return nullptr;
#endif

    //We search the children in an "intelligent" order (most-used
    //object types first), in order to avoid wasting too much time
    //looking for a match.  The most important part of this ordering
    //is that stars should be last (because the stars list is so long)
    SkyObject *o = nullptr;
    o            = m_SolarSystem->findByName(name);
    if (o)
        return o;
    o = m_DeepSky->findByName(name);
    if (o)
        return o;
    o = m_CustomCatalogs->findByName(name);
    if (o)
        return o;
    o = m_internetResolvedComponent->findByName(name);
    if (o)
        return o;
    o = m_manualAdditionsComponent->findByName(name);
    if (o)
        return o;
    o = m_CNames->findByName(name);
    if (o)
        return o;
    o = m_Stars->findByName(name);
    if (o)
        return o;
    o = m_Supernovae->findByName(name);
    if (o)
        return o;
    o = m_Satellites->findByName(name);
    if (o)
        return o;

    return nullptr;
}

SkyObject *SkyMapComposite::findStarByGenetiveName(const QString name)
{
    return m_Stars->findStarByGenetiveName(name);
}

KSPlanetBase *SkyMapComposite::planet(int n)
{
    if (n == KSPlanetBase::SUN)
        return (KSPlanetBase *)(m_SolarSystem->findByName("Sun"));
    if (n == KSPlanetBase::MERCURY)
        return (KSPlanetBase *)(m_SolarSystem->findByName(i18n("Mercury")));
    if (n == KSPlanetBase::VENUS)
        return (KSPlanetBase *)(m_SolarSystem->findByName(i18n("Venus")));
    if (n == KSPlanetBase::MOON)
        return (KSPlanetBase *)(m_SolarSystem->findByName("Moon"));
    if (n == KSPlanetBase::MARS)
        return (KSPlanetBase *)(m_SolarSystem->findByName(i18n("Mars")));
    if (n == KSPlanetBase::JUPITER)
        return (KSPlanetBase *)(m_SolarSystem->findByName(i18n("Jupiter")));
    if (n == KSPlanetBase::SATURN)
        return (KSPlanetBase *)(m_SolarSystem->findByName(i18n("Saturn")));
    if (n == KSPlanetBase::URANUS)
        return (KSPlanetBase *)(m_SolarSystem->findByName(i18n("Uranus")));
    if (n == KSPlanetBase::NEPTUNE)
        return (KSPlanetBase *)(m_SolarSystem->findByName(i18n("Neptune")));
    //if ( n == KSPlanetBase::PLUTO ) return (KSPlanetBase*)(m_SolarSystem->findByName( i18n( "Pluto" ) ) );

    return nullptr;
}

void SkyMapComposite::addCustomCatalog(const QString &filename, int index)
{
    CatalogComponent *cc = new CatalogComponent(this, filename, false, index);
    if (cc->objectList().size())
    {
        m_CustomCatalogs->addComponent(cc);
    }
    else
    {
        delete cc;
    }
}

void SkyMapComposite::removeCustomCatalog(const QString &name)
{
    foreach (SkyComponent *sc, m_CustomCatalogs->components())
    {
        CatalogComponent *ccc = (CatalogComponent *)sc;

        if (ccc->name() == name)
        {
            m_CustomCatalogs->removeComponent(ccc);
            return;
        }
    }

    qWarning() << i18n("Could not find custom catalog component named %1.", name);
}

void SkyMapComposite::reloadCLines()
{
#ifndef KSTARS_LITE
    Q_ASSERT(!SkyMapDrawAbstract::drawLock());
    SkyMapDrawAbstract::setDrawLock(
        true); // This is not (yet) multithreaded, so I think we don't have to worry about overwriting the state of an existing lock --asimha
    removeComponent(m_CLines);
    delete m_CLines;
    addComponent(m_CLines = new ConstellationLines(this, m_Cultures.get()));
    SkyMapDrawAbstract::setDrawLock(false);
#endif
}

void SkyMapComposite::reloadCNames()
{
    //     Q_ASSERT( !SkyMapDrawAbstract::drawLock() );
    //     SkyMapDrawAbstract::setDrawLock( true ); // This is not (yet) multithreaded, so I think we don't have to worry about overwriting the state of an existing lock --asimha
    //     objectNames(SkyObject::CONSTELLATION).clear();
    //     delete m_CNames;
    //     m_CNames = 0;
    //     m_CNames = new ConstellationNamesComponent( this, m_Cultures.get() );
    //     SkyMapDrawAbstract::setDrawLock( false );
    objectNames(SkyObject::CONSTELLATION).clear();
    removeComponent(m_CNames);
    delete m_CNames;
    addComponent(m_CNames = new ConstellationNamesComponent(this, m_Cultures.get()));
}

void SkyMapComposite::reloadConstellationArt()
{
#ifndef KSTARS_LITE
    Q_ASSERT(!SkyMapDrawAbstract::drawLock());
    SkyMapDrawAbstract::setDrawLock(true);
    removeComponent(m_ConstellationArt);
    delete m_ConstellationArt;
    addComponent(m_ConstellationArt = new ConstellationArtComponent(this, m_Cultures.get()));
    SkyMapDrawAbstract::setDrawLock(false);
#endif
}

void SkyMapComposite::reloadDeepSky()
{
#ifndef KSTARS_LITE
    Q_ASSERT(!SkyMapDrawAbstract::drawLock());

    // Deselect object if selected! If not deselected then InfoBox tries to
    // get the name of an object which may not exist (getLongName)
    // FIXME (spacetime): Is there a better way?
    // Current Solution: Look for the nearest star in the region and select it.

    SkyMap *current_map   = KStars::Instance()->map();
    double maxrad         = 30.0;
    SkyPoint center_point = current_map->getCenterPoint();

    current_map->setClickedObject(KStars::Instance()->data()->skyComposite()->starNearest(&center_point, maxrad));
    current_map->setClickedPoint(current_map->clickedObject());
    current_map->slotCenter();

    // Remove and Regenerate set of catalog objects
    //
    // FIXME: Why should we do this? Because it messes up observing
    // list really bad to delete and regenerate SkyObjects.
    SkyMapDrawAbstract::setDrawLock(true);
    m_CustomCatalogs.reset(new SkyComposite(this));
    removeComponent(m_internetResolvedComponent);
    delete m_internetResolvedComponent;
    addComponent(m_internetResolvedComponent = new SyncedCatalogComponent(this, m_internetResolvedCat, true, 0), 6);
    removeComponent(m_manualAdditionsComponent);
    delete m_manualAdditionsComponent;
    addComponent(m_manualAdditionsComponent = new SyncedCatalogComponent(this, m_manualAdditionsCat, true, 0), 6);
    QStringList allcatalogs = Options::showCatalogNames();

    for (int i = 0; i < allcatalogs.size(); ++i)
    {
        if (allcatalogs.at(i) == m_internetResolvedCat ||
            allcatalogs.at(i) == m_manualAdditionsCat) // These are special catalogs
            continue;
        m_CustomCatalogs->addComponent(new CatalogComponent(this, allcatalogs.at(i), false, i),
                                       5); // FIXME: Should this be 6 or 5? See SkyMapComposite::SkyMapComposite()
    }

    // FIXME: We should also reload anything that refers to SkyObject
    // * in memory, because all the old SkyObjects are now gone! This
    // includes the observing list. Otherwise, expect a bad, bad crash
    // that is hard to debug! -- AS

    SkyMapDrawAbstract::setDrawLock(false);
#endif
}

bool SkyMapComposite::isLocalCNames()
{
    return m_CNames->isLocalCNames();
}

void SkyMapComposite::emitProgressText(const QString &message)
{
    emit progressText(message);
#ifndef Q_OS_ANDROID
    //Can cause crashes on Android, investigate it
    qApp->processEvents(); // -jbb: this seemed to make it work.
#endif
    //qCDebug(KSTARS) << QString("PROGRESS TEXT: %1\n").arg( message );
}

const QList<DeepSkyObject *> &SkyMapComposite::deepSkyObjects() const
{
    return m_DeepSky->objectList();
}

const QList<SkyObject *> &SkyMapComposite::constellationNames() const
{
    return m_CNames->objectList();
}

// Returns only named stars, and should not be used
const QList<SkyObject *> &SkyMapComposite::stars() const
{
    return m_Stars->objectList();
}

const QList<SkyObject *> &SkyMapComposite::asteroids() const
{
    return m_SolarSystem->asteroids();
}

const QList<SkyObject *> &SkyMapComposite::comets() const
{
    return m_SolarSystem->comets();
}

const QList<SkyObject *> &SkyMapComposite::supernovae() const
{
    return m_Supernovae->objectList();
}

QList<SkyObject *> SkyMapComposite::planets()
{
    return solarSystemComposite()->planetObjects();
}

//Store it permanently
/*
QList<SkyObject*> SkyMapComposite::moons()
{
    QList<SkyObject*> skyObjects;
    foreach(PlanetMoonsComponent *pMoons, m_SolarSystem->planetMoonsComponent()) {
        PlanetMoons *moons = pMoons->getMoons();
        for(int i = 0; i < moons->nMoons(); ++i) {
            skyObjects.append(moons->moon(i));
        }
    }
    return skyObjects;
}
*/

const QList<SkyObject *> *SkyMapComposite::getSkyObjectsList(SkyObject::TYPE t)
{
    switch (t)
    {
        case SkyObject::STAR:
            return &m_Stars->objectList();
            break;
        case SkyObject::CATALOG_STAR:
            return nullptr;
            break;
        case SkyObject::PLANET:
            return &m_SolarSystem->planetObjects();
            break;
        case SkyObject::COMET:
            return &comets();
            break;
        case SkyObject::ASTEROID:
            return &asteroids();
            break;
        case SkyObject::MOON:
            return &m_SolarSystem->moons();
            break;
        case SkyObject::GALAXY:
        case SkyObject::PLANETARY_NEBULA:
        case SkyObject::GASEOUS_NEBULA:
        case SkyObject::GLOBULAR_CLUSTER:
        case SkyObject::OPEN_CLUSTER:
            return nullptr;
            break;
        case SkyObject::CONSTELLATION:
            return &constellationNames();
            break;
        case SkyObject::SUPERNOVA:
            return &supernovae();
            break;
        default:
            return nullptr;
    }
    //return nullptr;
}

KSPlanet *SkyMapComposite::earth()
{
    return m_SolarSystem->earth();
}

QList<SkyComponent *> SkyMapComposite::customCatalogs()
{
    return m_CustomCatalogs->components();
}

QStringList SkyMapComposite::getCultureNames()
{
    return m_Cultures->getNames();
}

QString SkyMapComposite::getCultureName(int index)
{
    return m_Cultures->getName(index);
}

void SkyMapComposite::setCurrentCulture(QString culture)
{
    m_Cultures->setCurrent(culture);
}

QString SkyMapComposite::currentCulture()
{
    return m_Cultures->current();
}
#ifndef KSTARS_LITE
FlagComponent *SkyMapComposite::flags()
{
    return m_Flags;
}
#endif

SatellitesComponent *SkyMapComposite::satellites()
{
    return m_Satellites;
}

SupernovaeComponent *SkyMapComposite::supernovaeComponent()
{
    return m_Supernovae;
}

ArtificialHorizonComponent *SkyMapComposite::artificialHorizon()
{
    return m_ArtificialHorizon;
}
