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
	if ( pointList().size() < 2 ) return;

	SkyMap *map = ks->map();

	//Draw Constellation Lines
	psky.setPen( QPen( QColor( ks->data()->colorScheme()->colorNamed( "CLineColor" ) ), 1, Qt::SolidLine ) ); //change to CLine color
//	int iLast = -1;

	SkyPoint *pLast = pointList().at(0);
	for ( int i=1; i < pointList().size(); ++i ) {
        SkyPoint *pThis = pointList().at(i);
        if (m_CLineModeList.at(i) == 'D') {
            map->drawClippedLine( pLast, pThis, psky, scale);
        }
        pLast = pThis;
    }
}
