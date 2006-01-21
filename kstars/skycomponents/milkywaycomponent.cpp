/***************************************************************************
                          milkywaycomponent.cpp  -  K Desktop Planetarium
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

#include "horizoncomponent.h"

#include <QList>
#include <QPointF>
#include <QPolygonF>
#include <QPainter>
#include <Q3PointArray>
#include <QFile>

#include "skycomposite.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "skymap.h"
#include "skypoint.h" 
#include "dms.h"
#include "Options.h"
#include "ksutils.h"
#include "ksfilereader.h"

#include "milkywaycomponent.h"

MilkyWayComponent::MilkyWayComponent(SkyComponent *parent, const QString &fileName, bool (*visibleMethod)()) 
: PointListComponent(parent, visibleMethod), m_FileName( fileName )
{
}

MilkyWayComponent::~MilkyWayComponent()
{
}

void MilkyWayComponent::init(KStarsData *)
{
	emitProgressText( i18n("Loading milky way" ) );
	QFile file;

	if ( KSUtils::openDataFile( file, m_FileName ) ) {
		KSFileReader fileReader( file ); // close file is included
		while ( fileReader.hasMoreLines() ) {
			QString line;
			double ra, dec;

			line = fileReader.readLine();

			ra = line.mid( 0, 8 ).toDouble();
			dec = line.mid( 9, 8 ).toDouble();

			SkyPoint *o = new SkyPoint( ra, dec );
			MilkyWay.append( o );
		}
	}
}

void MilkyWayComponent::draw(KStars *ks, QPainter& psky, double scale)
{
	if (! visible()) return;
	
	SkyMap *map = ks->map();

	float mwmax = scale * Options::zoomFactor()/100.;
	float Width = scale * map->width();
	float Height = scale * map->height();

	int thick(1);
	if ( ! Options::fillMilkyWay() ) thick=3;

	psky.setPen( QPen( QColor( ks->data()->colorScheme()->colorNamed( "MWColor" ) ), thick, Qt::SolidLine ) );
	psky.setBrush( QBrush( QColor( ks->data()->colorScheme()->colorNamed( "MWColor" ) ) ) );

	//Draw filled Milky Way: construct a QPolygonF from the SkyPoints, then draw it onscreen
	if ( Options::fillMilkyWay() ) {
		QPolygonF polyMW;
		bool partVisible = false;

		foreach ( SkyPoint *p, MilkyWay ) {
			QPointF o = map->getXY( p, Options::useAltAz(), Options::useRefraction(), scale );
			if ( o.x() > -1000000. && o.y() > -1000000. ) polyMW << o;
			if ( o.x() >= 0. && o.x() <= Width && o.y() >= 0. && o.y() <= Height ) partVisible = true;
		}

		if ( polyMW.size() && partVisible ) psky.drawPolygon( polyMW );

	//Draw Milky Way outline.  To prevent drawing seams between MW chunks, 
	//Only draw a line if the endpoints are close together
	} else {
		bool onscreen, lastonscreen=false;
		QPointF o, oLast;

		foreach ( SkyPoint *p, MilkyWay ) {
			o = map->getXY( p, Options::useAltAz(), Options::useRefraction(), scale );
			if (o.x() < -1000000. && o.y() < -1000000.) onscreen = false;
			else onscreen = true;

			if ( onscreen && lastonscreen ) {
				float dx = fabs(o.x() - oLast.x());
				float dy = fabs(o.y() - oLast.y());
				if ( dx<mwmax && dy<mwmax ) 
					psky.drawLine( oLast, o );
			}

			oLast = o;
			lastonscreen = onscreen;
		}
	}
}
