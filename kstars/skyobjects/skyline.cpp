/***************************************************************************
                          skyline.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Mon June 26 2006
    copyright            : (C) 2006 by Jason Harris
    email                : kstarss@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "skyline.h"
#include "kstarsdata.h"
#include "ksnumbers.h"

SkyLine::SkyLine()
{}

SkyLine::~SkyLine() {
    clear();
}

void SkyLine::clear() {
    qDeleteAll( m_pList );
    m_pList.clear();
}

void SkyLine::append( SkyPoint *p ) {
    m_pList.append( new SkyPoint( *p ) );
}

void SkyLine::setPoint( int i, SkyPoint *p ) {
    if ( i < 0 || i >= m_pList.size() ) {
        qDebug() << xi18n("SkyLine index error: no such point: %1", i );
        return;
    }
    *m_pList[i] = *p;
}

dms SkyLine::angularSize( int i ) const{
    if ( i < 0 || i+1 >= m_pList.size() ) {
        qDebug() << xi18n("SkyLine index error: no such segment: %1", i );
        return dms();
    }

    SkyPoint *p1 = m_pList[i];
    SkyPoint *p2 = m_pList[i+1];
    double dalpha = p1->ra().radians() - p2->ra().radians();
    double ddelta = p1->dec().radians() - p2->dec().radians() ;

    double sa = sin(dalpha/2.);
    double sd = sin(ddelta/2.);

    double hava = sa*sa;
    double havd = sd*sd;

    double aux = havd + cos (p1->dec().radians()) * cos(p2->dec().radians()) * hava;

    dms angDist;
    angDist.setRadians( 2 * fabs(asin( sqrt(aux) )) );

    return angDist;
}

void SkyLine::update( KStarsData *d, KSNumbers *num ) {
    foreach ( SkyPoint *p, m_pList ) {
        if ( num )
            p->updateCoords( num );
        p->EquatorialToHorizontal( d->lst(), d->geo()->lat() );
    }
}
