/***************************************************************************
                             ecliptic.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 2007-08-09
    copyright            : (C) 2007 by James B. Bowlin
    email                : bowlin@mindspring.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "ecliptic.h"

#include <QPainter>

#include "ksnumbers.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "skymap.h"
#include "skypoint.h" 
#include "dms.h"
#include "Options.h"
#include "linelist.h"

Ecliptic::Ecliptic(SkyComponent *parent ) : 
   LineListIndex( parent, i18n("Ecliptic") ), 
	m_label( LineListIndex::name() )
{}


bool Ecliptic::selected()
{
    return Options::showEcliptic();
}

void Ecliptic::draw( KStars *kstars, QPainter &psky, double scale )
{
	QColor color( kstars->data()->colorScheme()->colorNamed( "EclColor" ) );
	psky.setPen( QPen( QBrush( color ), 1, Qt::SolidLine ) );

	m_label.reset( psky );

	if ( ! skyMesh()->isZoomedIn() ) {
		if ( Options::useAntialias() )
			drawLinesFloat( kstars, psky, scale );
		else 
			drawLinesInt( kstars, psky, scale );
	}
	else {
		if ( Options::useAntialias() )
			drawAllLinesFloat( kstars, psky, scale );
		else 
			drawAllLinesInt( kstars, psky, scale );
	}

	m_label.draw( kstars, psky, scale );
}

void Ecliptic::init(KStarsData *data)
{
	KSNumbers num( data->ut().djd() );
	dms elat(0.0), elng(0.0);

	double eps    =   0.1;
    double minRa  =   0.0;
    double maxRa  =  23.0;
    double dRa    =   2.0;
	double dRa2   =  2. / 5.;
	double ra, ra2;

	for ( ra = minRa; ra < maxRa; ra += dRa ) {
		LineList* lineList = new LineList();
		for ( ra2 = ra; ra2 <= ra + dRa + eps; ra2 += dRa2 ) {
			elng.setH( ra2 );
			SkyPoint* o = new SkyPoint();
			o->setFromEcliptic( num.obliquity(), &elng, &elat );
			o->setRA0( o->ra()->Hours() );
			o->setDec0( o->dec()->Degrees() );
			o->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
			lineList->append( o );
		}
		appendLine( lineList );
	}
}

