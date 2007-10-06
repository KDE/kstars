/***************************************************************************
                          solarsystemcomposite.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/01/09
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

#include "solarsystemcomposite.h"

#include <QPainter>
#include <klocale.h>

#include "solarsystemsinglecomponent.h"
#include "asteroidscomponent.h"
#include "cometscomponent.h"
#include "skycomponent.h"

#include "Options.h"
#include "kstars.h"
#include "skymap.h"
#include "kstarsdata.h"
#include "ksnumbers.h"
#include "ksplanet.h"
#include "kssun.h"
#include "ksmoon.h"
#include "kspluto.h"
#include "jupitermoonscomponent.h"


SolarSystemComposite::SolarSystemComposite(SkyComponent *parent, KStarsData *data )
        : SkyComposite(parent)
{

    m_Earth = new KSPlanet( data, I18N_NOOP( "Earth" ), QString(), QColor( "white" ), 12756.28 /*diameter in km*/ );

    m_Sun = new KSSun(data);
    addComponent( new SolarSystemSingleComponent( this, m_Sun, Options::showSun, 8 ) );
    m_Moon = new KSMoon(data);
    addComponent( new SolarSystemSingleComponent( this, m_Moon, Options::showMoon, 8 ) );
    addComponent( new SolarSystemSingleComponent( this, new KSPlanet( data, I18N_NOOP("Mercury"), "mercury.png", QColor( "slateblue" ), 4879.4 /*diameter in km*/ ), Options::showMercury, 4 ) );
    addComponent( new SolarSystemSingleComponent( this, new KSPlanet( data, I18N_NOOP("Venus"), "venus.png", QColor( "lightgreen" ), 12103.6 /*diameter in km*/ ), Options::showVenus, 4 ) );
    addComponent( new SolarSystemSingleComponent( this, new KSPlanet( data, I18N_NOOP( "Mars" ), "mars.png", QColor( "red" ), 6792.4 /*diameter in km*/ ), Options::showMars, 4 ) );
    SolarSystemSingleComponent *jup = new SolarSystemSingleComponent( this, new KSPlanet( data, I18N_NOOP( "Jupiter" ), "jupiter.png", QColor( "goldenrod" ), 142984. /*diameter in km*/ ), Options::showJupiter, 4 );
    addComponent( jup );
    m_JupiterMoons = new JupiterMoonsComponent( this, jup, &Options::showJupiter);
    addComponent( m_JupiterMoons );
    addComponent( new SolarSystemSingleComponent( this, new KSPlanet( data, I18N_NOOP( "Saturn" ), "saturn.png", QColor( "khaki" ), 120536. /*diameter in km*/ ), Options::showSaturn, 4 ) );
    addComponent( new SolarSystemSingleComponent( this, new KSPlanet( data, I18N_NOOP( "Uranus" ), "uranus.png", QColor( "lightseagreen" ), 51118. /*diameter in km*/ ), Options::showUranus, 4 ) );
    addComponent( new SolarSystemSingleComponent( this, new KSPlanet( data, I18N_NOOP( "Neptune" ), "neptune.png", QColor( "skyblue" ), 49572. /*diameter in km*/ ), Options::showNeptune, 4 ) );
    addComponent( new SolarSystemSingleComponent( this, new KSPluto(data), Options::showPluto, 4 ) );

    m_AsteroidsComponent = new AsteroidsComponent( this, Options::showAsteroids );
    addComponent( m_AsteroidsComponent );
    m_CometsComponent = new CometsComponent( this, Options::showComets );
    addComponent( m_CometsComponent );
}

SolarSystemComposite::~SolarSystemComposite()
{
    delete m_Earth;
}

bool SolarSystemComposite::selected()
{
    return Options::showSolarSystem() &&
           ! ( Options::hideOnSlew() && Options::hidePlanets() && SkyMap::IsSlewing() );
}

void SolarSystemComposite::init(KStarsData *data)
{
    if (!m_Earth->loadData())
        return; //stop initializing

    emitProgressText( i18n("Loading solar system" ) );

    //init all sub components
    SkyComposite::init(data);
}

void SolarSystemComposite::update( KStarsData *data, KSNumbers *num )
{
    if ( ! selected() ) return;

    m_Sun->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
    m_Moon->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
    m_JupiterMoons->update( data, num );

    foreach ( SkyComponent *comp, components() ) {
        comp->update( data, num );
    }
}

void SolarSystemComposite::updatePlanets( KStarsData *data, KSNumbers *num )
{
    if ( ! selected() ) return;

    m_Earth->findPosition( num );
    foreach ( SkyComponent *comp, components() ) {
        comp->updatePlanets( data, num );
    }
}

void SolarSystemComposite::updateMoons( KStarsData *data, KSNumbers *num )
{
    if ( ! selected() ) return;

    m_Sun->findPosition( num );
    m_Moon->findPosition( num, data->geo()->lat(), data->lst() );
    m_Moon->findPhase( m_Sun );
    m_JupiterMoons->updateMoons( data, num );
}

void SolarSystemComposite::draw( KStars *ks, QPainter& psky )
{
    if ( ! selected() ) return;

    //FIXME: first draw the objects which are far away
    //(Thomas had been doing this by keeping separate pointers to
    //inner solar system objects, but I'd rather handle it here in the draw
    //function if possible
    SkyComposite::draw( ks, psky );
}

void SolarSystemComposite::drawTrails(KStars *ks, QPainter& psky )
{
    if ( ! selected() ) return;

    foreach ( SkyComponent *comp, components() ) {
        comp->drawTrails( ks, psky );
    }
}

QList<SkyObject*>& SolarSystemComposite::asteroids() {
    return m_AsteroidsComponent->objectList();
}

QList<SkyObject*>& SolarSystemComposite::comets() {
    return m_CometsComponent->objectList();
}

void SolarSystemComposite::reloadAsteroids( KStarsData *data ) {
    m_AsteroidsComponent->clear();
    m_AsteroidsComponent->init( data );
}

void SolarSystemComposite::reloadComets( KStarsData *data ) {
    m_CometsComponent->clear();
    m_CometsComponent->init( data );
}
