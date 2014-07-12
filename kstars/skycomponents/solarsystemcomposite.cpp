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

#include <KLocalizedString>

#include "solarsystemsinglecomponent.h"
#include "asteroidscomponent.h"
#include "cometscomponent.h"
#include "skycomponent.h"

#include "Options.h"
#include "skymap.h"
#include "kstarsdata.h"
#include "ksnumbers.h"
#include "skyobjects/ksplanet.h"
#include "skyobjects/kssun.h"
#include "skyobjects/ksmoon.h"
#include "skyobjects/kspluto.h"
#include "planetmoonscomponent.h"

SolarSystemComposite::SolarSystemComposite(SkyComposite *parent ) :
    SkyComposite(parent)
{
    emitProgressText( xi18n("Loading solar system" ) );
    m_Earth = new KSPlanet( I18N_NOOP( "Earth" ), QString(), QColor( "white" ), 12756.28 /*diameter in km*/ );

    m_Sun = new KSSun();
    addComponent( new SolarSystemSingleComponent( this, m_Sun, Options::showSun ) );
    m_Moon = new KSMoon();
    addComponent( new SolarSystemSingleComponent( this, m_Moon, Options::showMoon ) );
    addComponent( new SolarSystemSingleComponent( this, new KSPlanet( KSPlanetBase::MERCURY ), Options::showMercury ) );
    addComponent( new SolarSystemSingleComponent( this, new KSPlanet( KSPlanetBase::VENUS ), Options::showVenus ) );
    addComponent( new SolarSystemSingleComponent( this, new KSPlanet( KSPlanetBase::MARS ), Options::showMars ) );
    SolarSystemSingleComponent *jup = new SolarSystemSingleComponent( this, new KSPlanet( KSPlanetBase::JUPITER ), Options::showJupiter );
    addComponent( jup );
    m_JupiterMoons = new PlanetMoonsComponent( this, jup, KSPlanetBase::JUPITER);
    addComponent( m_JupiterMoons );
    SolarSystemSingleComponent *sat = new SolarSystemSingleComponent( this, new KSPlanet( KSPlanetBase::SATURN ), Options::showSaturn );
    addComponent( sat );
    addComponent( new SolarSystemSingleComponent( this, new KSPlanet( KSPlanetBase::URANUS ), Options::showUranus ) );
    addComponent( new SolarSystemSingleComponent( this, new KSPlanet( KSPlanetBase::NEPTUNE ), Options::showNeptune ) );
    addComponent( new SolarSystemSingleComponent( this, new KSPluto(), Options::showPluto ) );

    addComponent( m_AsteroidsComponent = new AsteroidsComponent( this  ));
    addComponent( m_CometsComponent    = new CometsComponent( this ));
}

SolarSystemComposite::~SolarSystemComposite()
{
    delete m_Earth;
}

bool SolarSystemComposite::selected()
{
    return Options::showSolarSystem() &&
           !( Options::hideOnSlew() && Options::hidePlanets() && SkyMap::IsSlewing() );
}

void SolarSystemComposite::update( KSNumbers *num )
{
	//    if ( ! selected() ) return;

    KStarsData *data = KStarsData::Instance(); 
    m_Sun->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
    m_Moon->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
    m_JupiterMoons->update( num );

    foreach ( SkyComponent *comp, components() ) {
        comp->update( num );
    }
}

void SolarSystemComposite::updatePlanets( KSNumbers *num )
{
    m_Earth->findPosition( num );
    foreach ( SkyComponent *comp, components() ) {
        comp->updatePlanets( num );
    }
}

void SolarSystemComposite::updateMoons( KSNumbers *num )
{
	//    if ( ! selected() ) return;
    KStarsData *data = KStarsData::Instance(); 
    m_Sun->findPosition( num );
    m_Moon->findPosition( num, data->geo()->lat(), data->lst() );
    m_Moon->findPhase();
    m_JupiterMoons->updateMoons( num );
}

void SolarSystemComposite::drawTrails( SkyPainter* skyp )
{
    if( selected() )
        foreach( SkyComponent *comp, components() )
            comp->drawTrails( skyp );
}

const QList<SkyObject*>& SolarSystemComposite::asteroids() const {
    return m_AsteroidsComponent->objectList();
}

const QList<SkyObject*>& SolarSystemComposite::comets() const {
    return m_CometsComponent->objectList();
}

CometsComponent* SolarSystemComposite::cometsComponent()
{
    return m_CometsComponent;
}

AsteroidsComponent* SolarSystemComposite::asteroidsComponent()
{
    return m_AsteroidsComponent;
}
