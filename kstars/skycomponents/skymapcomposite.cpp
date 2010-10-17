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

#include <QPolygonF>
#include <QApplication>

#include "Options.h"
#include "kstarsdata.h"
#include "skymap.h"
#include "skyobjects/starobject.h"
#include "skyobjects/deepskyobject.h"
#include "skyobjects/ksplanet.h"

#include "targetlistcomponent.h"
#include "constellationboundarylines.h"
#include "constellationlines.h"
#include "culturelist.h"
#include "constellationnamescomponent.h"
#include "coordinategrid.h"
#include "customcatalogcomponent.h"
#include "deepskycomponent.h"
#include "equator.h"
#include "ecliptic.h"
#include "horizoncomponent.h"
#include "milkyway.h"
#include "solarsystemcomposite.h"
#include "starcomponent.h"
#include "deepstarcomponent.h"
#include "satellitecomposite.h"
#include "flagcomponent.h"


#include "skymesh.h"
#include "skylabeler.h"

#include "typedef.h"

SkyMapComposite::SkyMapComposite(SkyComposite *parent ) :
        SkyComposite(parent), m_reindexNum( J2000 )
{
    m_skyLabeler = SkyLabeler::Instance();

    m_skyMesh = SkyMesh::Create( 3 );  // level 5 mesh = 8192 trixels
    m_skyMesh->debug( 0 );
    //  1 => print "indexing ..."
    //  2 => prints totals too
    // 10 => prints detailed lists
    // You can also set the debug level of individual
    // appendLine() and appendPoly() calls.

    //Add all components
    //Stars must come before constellation lines
    addComponent( m_MilkyWay       = new MilkyWay( this ));
    addComponent( m_Stars          = StarComponent::Create( this ));
    addComponent( m_CoordinateGrid = new CoordinateGrid( this ));

    // Do add to components.
    addComponent( m_CBoundLines = new ConstellationBoundaryLines( this ));
    m_Cultures = new CultureList();
    addComponent( m_CLines     = new ConstellationLines( this, m_Cultures ));
    addComponent( m_CNames     = new ConstellationNamesComponent( this, m_Cultures ));
    addComponent( m_Equator    = new Equator( this ));
    addComponent( m_Ecliptic   = new Ecliptic( this ));
    addComponent( m_Horizon    = new HorizonComponent( this ));
    // Disable satellites for now
    // addComponent( m_Satellites = new SatelliteComposite( this ));
    addComponent( m_DeepSky    = new DeepSkyComponent( this ));

    m_CustomCatalogs = new SkyComposite( this );
    for ( int i=0; i<Options::catalogFile().size(); ++ i ) {
        m_CustomCatalogs->addComponent(
            new CustomCatalogComponent( this, Options::catalogFile()[i], false, i )
            );
    }

    addComponent( m_SolarSystem = new SolarSystemComposite( this ));
    addComponent( m_Flags       = new FlagComponent( this ));

    addComponent( m_ObservingList = new TargetListComponent( this , 0, QPen(),
                                                             &Options::obsListSymbol, &Options::obsListText ) );
        
    connect( this, SIGNAL( progressText( const QString & ) ),
             KStarsData::Instance(), SIGNAL( progressText( const QString & ) ) );
}

SkyMapComposite::~SkyMapComposite()
{
    delete m_skyLabeler;     // These are on the heap to avoid header file hell.
    delete m_skyMesh;
    delete m_Cultures;
    delete m_Flags;
}

void SkyMapComposite::update(KSNumbers *num )
{
    //printf("updating SkyMapComposite\n");
    //1. Milky Way
    //m_MilkyWay->update( data, num );
    //2. Coordinate grid
    //m_CoordinateGrid->update( data, num );
    //3. Constellation boundaries
    //m_CBounds->update( data, num );
    //4. Constellation lines
    //m_CLines->update( data, num );
    //5. Constellation names
    m_CNames->update( num );
    //6. Equator
    //m_Equator->update( data, num );
    //7. Ecliptic
    //m_Ecliptic->update( data, num );
    //8. Deep sky
    //m_DeepSky->update( data, num );
    //9. Custom catalogs
    m_CustomCatalogs->update( num );
    //10. Stars
    //m_Stars->update( data, num );
    //m_CLines->update( data, num );  // MUST follow stars.

    //12. Solar system
    m_SolarSystem->update( num );
    //13. Satellites
    //m_Satellites->update( data, num );
    //14. Horizon
    m_Horizon->update( num );
}

void SkyMapComposite::updatePlanets(KSNumbers *num )
{
    m_SolarSystem->updatePlanets( num );
}

void SkyMapComposite::updateMoons(KSNumbers *num )
{
    m_SolarSystem->updateMoons( num );
}

//Reimplement draw function so that we have control over the order of
//elements, and we can add object labels
//
//The order in which components are drawn naturally determines the
//z-ordering (the layering) of the components.  Objects which
//should appear "behind" others should be drawn first.
void SkyMapComposite::draw( QPainter& psky )
{
    QTime t;
    t.start();
    SkyMap *map = SkyMap::Instance();
    KStarsData *data = KStarsData::Instance();

    // We delay one draw cycle before re-indexing
    // we MUST ensure CLines do not get re-indexed while we use DRAW_BUF
    // so we do it here.
    m_CLines->reindex( &m_reindexNum );
    // This queues re-indexing for the next draw cycle
    m_reindexNum = KSNumbers( data->updateNum()->julianDay() );

    // This ensures that the JIT updates are synchronized for the entire draw
    // cycle so the sky moves as a single sheet.  May not be needed.
    data->syncUpdateIDs();

    // prepare the aperture
    float radius = map->fov();
    if ( radius > 90.0 )
        radius = 90.0;

    if ( m_skyMesh->inDraw() ) {
        printf("Warning: aborting concurrent SkyMapComposite::draw()\n");
        return;
    }

    m_skyMesh->inDraw( true );
    SkyPoint* focus = map->focus();
    m_skyMesh->aperture( focus, radius + 1.0, DRAW_BUF ); // divide by 2 for testing

    // create the no-precess aperture if needed
    if ( Options::showGrid() || Options::showCBounds() || Options::showEquator() ) {
        m_skyMesh->index( focus, radius + 1.0, NO_PRECESS_BUF );
    }

    // clear marks from old labels and prep fonts
    m_skyLabeler->reset( map, psky );
    m_skyLabeler->useStdFont( psky );

    // info boxes have highest label priority
    // FIXME: REGRESSION. Labeler now know nothing about infoboxes
    // map->infoBoxes()->reserveBoxes( psky );

    m_MilkyWay->draw( psky );

    m_CoordinateGrid->draw( psky );

    // Draw constellation boundary lines only if we draw western constellations
    if ( m_Cultures->current() == "Western" )
        m_CBoundLines->draw( psky );

    m_CLines->draw( psky );

    m_Equator->draw( psky );

    m_Ecliptic->draw( psky );

    m_DeepSky->draw( psky );

    m_CustomCatalogs->draw( psky );

    m_Stars->draw( psky );

    m_SolarSystem->drawTrails( psky );
    m_SolarSystem->draw( psky );

    // TODO: Fix satellites sometime
    //    m_Satellites->draw( psky );

    m_Horizon->draw( psky );

    map->drawObjectLabels( labelObjects(), psky );

    m_skyLabeler->drawQueuedLabels( psky );
    m_CNames->draw( psky );
    m_Stars->drawLabels( psky );
    m_DeepSky->drawLabels( psky );

    m_Flags->draw( psky );

    m_ObservingList->pen = QPen( QColor(data->colorScheme()->colorNamed( "ObsListColor" )), int(SkyMap::Instance()->scale()) );
    if( !m_ObservingList->list )
        m_ObservingList->list = &KStars::Instance()->observingList()->sessionList();
    m_ObservingList->draw( psky );

    m_skyMesh->inDraw( false );

    //kDebug() << QString("draw took %1 ms").arg( t.elapsed() );

    // -jbb uncomment these to see trixel outlines:
    //
    //psky.setPen(  QPen( QBrush( QColor( "yellow" ) ), 1, Qt::SolidLine ) );
    //m_skyMesh->draw( psky, OBJ_NEAREST_BUF );

    //psky.setPen( QPen( QBrush( QColor( "green" ) ), 1, Qt::SolidLine ) );
    //m_skyMesh->draw( psky, NO_PRECESS_BUF );
}
//Select nearest object to the given skypoint, but give preference
//to certain object types.
//we multiply each object type's smallest angular distance by the
//following factors before selecting the final nearest object:
// faint stars = 1.0 (not weighted)
// stars brighter than 4th mag = 0.75
// IC catalog = 0.8
// NGC catalog = 0.6
// "other" catalog = 0.6
// Messier object = 0.5
// custom object = 0.5
// Solar system = 0.25
SkyObject* SkyMapComposite::objectNearest( SkyPoint *p, double &maxrad ) {
    double rTry = maxrad;
    double rBest = maxrad;
    SkyObject *oTry = 0;
    SkyObject *oBest = 0;

    //printf("%.1f %.1f\n", p->ra().Degrees(), p->dec().Degrees() );
    m_skyMesh->aperture( p, maxrad + 1.0, OBJ_NEAREST_BUF);

    oBest = m_Stars->objectNearest( p, rBest );
    //reduce rBest by 0.75 for stars brighter than 4th mag
    if ( oBest && oBest->mag() < 4.0 ) rBest *= 0.75;

    // TODO: Add support for deep star catalogs

    //m_DeepSky internally discriminates among deepsky catalogs
    //and renormalizes rTry
    oTry = m_DeepSky->objectNearest( p, rTry );
    if ( rTry < rBest ) {
        rBest = rTry;
        oBest = oTry;
    }

    for( int i = 0; i < m_DeepStars.size(); ++i ) {
      rTry = maxrad;
      oTry = m_DeepStars.at( i )->objectNearest( p, rTry );
      if( rTry < rBest ) {
	rBest = rTry;
	oBest = oTry;
      }
    }

    rTry = maxrad;
    oTry = m_CustomCatalogs->objectNearest( p, rTry );
    rTry *= 0.5;
    if ( rTry < rBest ) {
        rBest = rTry;
        oBest = oTry;
    }

    rTry = maxrad;
    oTry = m_SolarSystem->objectNearest( p, rTry );
    rTry *= 0.25;
    if ( rTry < rBest ) {
        rBest = rTry;
        oBest = oTry;
    }

    maxrad = rBest;
    return oBest; //will be 0 if no object nearer than maxrad was found

}

SkyObject* SkyMapComposite::starNearest( SkyPoint *p, double &maxrad ) {
    double rtry = maxrad;
    SkyObject *star = 0;

    m_skyMesh->aperture( p, maxrad + 1.0, OBJ_NEAREST_BUF);

    star = m_Stars->objectNearest( p, rtry );
    //reduce rBest by 0.75 for stars brighter than 4th mag
    if ( star && star->mag() < 4.0 ) rtry *= 0.75;

    // TODO: Add Deep Star Catalog support

    maxrad = rtry;
    return star;
}

bool SkyMapComposite::addNameLabel( SkyObject *o ) {
    if ( !o ) return false;
    labelObjects().append( o );
    return true;
}

bool SkyMapComposite::removeNameLabel( SkyObject *o ) {
    if ( !o ) return false;
    int index = labelObjects().indexOf( o );
    if ( index < 0 ) return false;
    labelObjects().removeAt( index );
    return true;
}

QHash<int, QStringList>& SkyMapComposite::getObjectNames() {
    return m_ObjectNames;
}

QList<SkyObject*> SkyMapComposite::findObjectsInArea( const SkyPoint& p1, const SkyPoint& p2 )
{
    const SkyRegion& region = m_skyMesh->skyRegion( p1, p2 );
    QList<SkyObject*> list;
    // call objectsInArea( QList<SkyObject*>&, const SkyRegion& ) for each of the
    // components of the SkyMapComposite
    if( m_Stars->selected() )
        m_Stars->objectsInArea( list, region );
    if( m_DeepSky->selected() )
        m_DeepSky->objectsInArea( list, region );
    return list;
}

SkyObject* SkyMapComposite::findByName( const QString &name ) {
    //We search the children in an "intelligent" order (most-used
    //object types first), in order to avoid wasting too much time
    //looking for a match.  The most important part of this ordering
    //is that stars should be last (because the stars list is so long)
    SkyObject *o = 0;
    o = m_SolarSystem->findByName( name );
    if ( o ) return o;
    o = m_DeepSky->findByName( name );
    if ( o ) return o;
    o = m_CustomCatalogs->findByName( name );
    if ( o ) return o;
    o = m_CNames->findByName( name );
    if ( o ) return o;
    o = m_Stars->findByName( name );
    if ( o ) return o;

    return 0;
}


SkyObject* SkyMapComposite::findStarByGenetiveName( const QString name ) {
    return m_Stars->findStarByGenetiveName( name );
}

KSPlanetBase* SkyMapComposite::planet( int n ) {
    if ( n == KSPlanetBase::SUN ) return (KSPlanetBase*)(m_SolarSystem->findByName( "Sun" ) );
    if ( n == KSPlanetBase::MERCURY ) return (KSPlanetBase*)(m_SolarSystem->findByName( i18n( "Mercury" ) ) );
    if ( n == KSPlanetBase::VENUS ) return (KSPlanetBase*)(m_SolarSystem->findByName( i18n( "Venus" ) ) );
    if ( n == KSPlanetBase::MOON ) return (KSPlanetBase*)(m_SolarSystem->findByName( "Moon" ) );
    if ( n == KSPlanetBase::MARS ) return (KSPlanetBase*)(m_SolarSystem->findByName( i18n( "Mars" ) ) );
    if ( n == KSPlanetBase::JUPITER ) return (KSPlanetBase*)(m_SolarSystem->findByName( i18n( "Jupiter" ) ) );
    if ( n == KSPlanetBase::SATURN ) return (KSPlanetBase*)(m_SolarSystem->findByName( i18n( "Saturn" ) ) );
    if ( n == KSPlanetBase::URANUS ) return (KSPlanetBase*)(m_SolarSystem->findByName( i18n( "Uranus" ) ) );
    if ( n == KSPlanetBase::NEPTUNE ) return (KSPlanetBase*)(m_SolarSystem->findByName( i18n( "Neptune" ) ) );
    if ( n == KSPlanetBase::PLUTO ) return (KSPlanetBase*)(m_SolarSystem->findByName( i18n( "Pluto" ) ) );

	return 0;
}

void SkyMapComposite::addCustomCatalog( const QString &filename, int index ) {
    CustomCatalogComponent *cc = new CustomCatalogComponent( this, filename, false, index );
    if( cc->objectList().size() ) {
        m_CustomCatalogs->addComponent( cc );
    } else {
        delete cc;
    }
}

void SkyMapComposite::removeCustomCatalog( const QString &name ) {
    foreach( SkyComponent *sc, m_CustomCatalogs->components() ) {
        CustomCatalogComponent *ccc = (CustomCatalogComponent*)sc;

        if ( ccc->name() == name ) {
            m_CustomCatalogs->removeComponent( ccc );
            return;
        }
    }

    kWarning() << i18n( "Could not find custom catalog component named %1." , name) ;
}

void SkyMapComposite::reloadCLines( ) {
    delete m_CLines;
    m_CLines = new ConstellationLines( this, m_Cultures );
}

void SkyMapComposite::reloadCNames( ) {
    objectNames(SkyObject::CONSTELLATION).clear();
    delete m_CNames;
    m_CNames = new ConstellationNamesComponent( this, m_Cultures );
}

bool SkyMapComposite::isLocalCNames() {
    return m_CNames->isLocalCNames();
}

void SkyMapComposite::emitProgressText( const QString &message ) {
    emit progressText( message );
    qApp->processEvents();         // -jbb: this seemed to make it work.
    //kDebug() << QString("PROGRESS TEXT: %1\n").arg( message );
}

const QList<DeepSkyObject*>& SkyMapComposite::deepSkyObjects() const {
    return m_DeepSky->objectList();
}

const QList<SkyObject*>& SkyMapComposite::constellationNames() const {
    return m_CNames->objectList();
}

// Returns only named stars, and should not be used
const QList<SkyObject*>& SkyMapComposite::stars() const {
    return m_Stars->objectList();
}


const QList<SkyObject*>& SkyMapComposite::asteroids() const {
    return m_SolarSystem->asteroids();
}

const QList<SkyObject*>& SkyMapComposite::comets() const {
    return m_SolarSystem->comets();
}

KSPlanet* SkyMapComposite::earth() {
    return m_SolarSystem->earth();
}

QList<SkyComponent*> SkyMapComposite::customCatalogs() {
    return m_CustomCatalogs->components();
}

QStringList SkyMapComposite::getCultureNames() {
    return m_Cultures->getNames();
}

QString SkyMapComposite::getCultureName( int index ) {
    return m_Cultures->getName( index );
}

void SkyMapComposite::setCurrentCulture( QString culture ) {
    m_Cultures->setCurrent( culture );
}

QString SkyMapComposite::currentCulture() {
    return m_Cultures->current();
}

FlagComponent* SkyMapComposite::flags() {
    return m_Flags;
}

#include "skymapcomposite.moc"
