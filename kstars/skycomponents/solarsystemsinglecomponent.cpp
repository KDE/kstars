/***************************************************************************
                          solarsystemsinglecomponent.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/30/08
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

#include "solarsystemsinglecomponent.h"
#include "solarsystemcomposite.h"
#include "skycomponent.h"


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

SolarSystemSingleComponent::SolarSystemSingleComponent(SolarSystemComposite *parent, KSPlanetBase *kspb, bool (*visibleMethod)()) :
    SkyComponent( parent ),
    visible( visibleMethod ),
    m_Earth( parent->earth() ),
    m_Planet( kspb )
{
    m_Planet->loadData();
    if ( ! m_Planet->name().isEmpty() )
        objectNames(m_Planet->type()).append( m_Planet->name() );
    if ( ! m_Planet->longname().isEmpty() && m_Planet->longname() != m_Planet->name() )
        objectNames(m_Planet->type()).append( m_Planet->longname() );
}

SolarSystemSingleComponent::~SolarSystemSingleComponent()
{
    removeFromNames( m_Planet );
    delete m_Planet;
}

bool SolarSystemSingleComponent::selected() {
    return visible();
}

SkyObject* SolarSystemSingleComponent::findByName( const QString &name ) {
    if( QString::compare( m_Planet->name(),     name, Qt::CaseInsensitive ) == 0 ||
        QString::compare( m_Planet->longname(), name, Qt::CaseInsensitive ) == 0 ||
        QString::compare( m_Planet->name2(),    name, Qt::CaseInsensitive ) == 0
        )
        return m_Planet;
    return 0;
}

SkyObject* SolarSystemSingleComponent::objectNearest( SkyPoint *p, double &maxrad ) {
    double r = m_Planet->angularDistanceTo( p ).Degrees();
    if( r < maxrad ) {
        maxrad = r;
        return m_Planet;
    }
    return 0;
}

void SolarSystemSingleComponent::update(KSNumbers*) {
    KStarsData *data = KStarsData::Instance(); 
    if( selected() )
        m_Planet->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
}

void SolarSystemSingleComponent::updateSolarSystemBodies(KSNumbers *num) {
    if ( selected() ) {
        KStarsData *data = KStarsData::Instance(); 
        m_Planet->findPosition( num, data->geo()->lat(), data->lst(), m_Earth );
        m_Planet->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
        if ( m_Planet->hasTrail() )
            m_Planet->updateTrail( data->lst(), data->geo()->lat() );
    }
}

void SolarSystemSingleComponent::draw( SkyPainter *skyp ) {
    if( ! selected() )
        return;

    skyp->setPen( m_Planet->color() );
    skyp->setBrush( m_Planet->color() );

    bool drawn = skyp->drawPlanet(m_Planet);
    if ( drawn && Options::showPlanetNames() )
        SkyLabeler::AddLabel( m_Planet, SkyLabeler::PLANET_LABEL );
}

void SolarSystemSingleComponent::drawTrails( SkyPainter *skyp ) {
    if( selected() )
        m_Planet->drawTrail(skyp);
}
