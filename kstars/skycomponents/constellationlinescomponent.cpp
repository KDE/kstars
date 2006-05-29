//Added by qt3to4:
#include <QTextStream>
/***************************************************************************
                          constellationlinescomponent.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/10/08
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

#include <QFile>
#include <QPainter>
#include <QTextStream>

#include "constellationlinescomponent.h"

#include "kstars.h"
#include "kstarsdata.h"
#include "ksutils.h"
#include "skymap.h"
#include "Options.h"

ConstellationLinesComponent::ConstellationLinesComponent(SkyComponent *parent, bool (*visibleMethod)())
: PointListComponent(parent, visibleMethod)
{
}

ConstellationLinesComponent::~ConstellationLinesComponent() {
}

void ConstellationLinesComponent::draw(KStars *ks, QPainter& psky, double scale)
{
	if ( !visible() ) return;

	SkyMap *map = ks->map();
	float Width = scale * map->width();
	float Height = scale * map->height();

	//Draw Constellation Lines
	psky.setPen( QPen( QColor( ks->data()->colorScheme()->colorNamed( "CLineColor" ) ), 1, Qt::SolidLine ) ); //change to CLine color
//	int iLast = -1;

	QPointF oStart;
	for ( int i=0; i < pointList().size(); ++i ) {
		QPointF o = map->toScreen( pointList().at(i), Options::projection(), Options::useAltAz(), Options::useRefraction(), scale );

		if ( ( o.x() >= -1000. && o.x() <= Width+1000. && o.y() >=-1000. && o.y() <= Height+1000. ) ) {
			if ( oStart.x() != 0 && m_CLineModeList.at(i)=='D' ) {
				psky.drawLine( oStart, o );
			}
			oStart = o;
		}
  }
}
