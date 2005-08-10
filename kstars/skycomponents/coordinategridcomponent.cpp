/***************************************************************************
                          milkywaycomponent.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/09/08
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

#include "coordinategridcomponent.h"

#include <QPoint>
#include <QPainter>

#include "skycomposite.h"
#include "kstarsdata.h"
#include "skymap.h"
#include "skypoint.h" 
#include "Options.h"

CoordinateGridComponent::CoordinateGridComponent(SkyComposite *parent) : SkyComponent(parent)
{
}

void SkyMap::draw(SkyMap *map, QPainter& psky, double scale)
{
// TODO add accessor methods to map for guideMax etc.

	if (!Options::showGrid()) return;
	
	QPoint cur;

	//Draw coordinate grid
	psky.setPen( QPen( QColor( map->data()->colorScheme()->colorNamed( "GridColor" ) ), 1, DotLine ) ); //change to GridColor

	//First, the parallels
	for ( double Dec=-80.; Dec<=80.; Dec += 20. ) {
		bool newlyVisible = false;
		sp->set( 0.0, Dec );
		if ( Options::useAltAz() ) sp->EquatorialToHorizontal( map->data()->LST, map->data()->geo()->lat() );
		QPoint o = map->getXY( sp, Options::useAltAz(), Options::useRefraction(), scale );
		QPoint o1 = o;
		cur = o;
		psky.moveTo( o.x(), o.y() );

		double dRA = 1./15.; //180 points along full circle of RA
		for ( double RA=dRA; RA<24.; RA+=dRA ) {
			sp->set( RA, Dec );
			if ( Options::useAltAz() ) sp->EquatorialToHorizontal( map->data()->LST, map->data()->geo()->lat() );

			if ( map->checkVisibility( sp, guideFOV, guideXRange ) ) {
				o = map->getXY( sp, Options::useAltAz(), Options::useRefraction(), scale );

				//When drawing on the printer, the psky.pos() point does NOT get updated
				//when lineTo or moveTo are called.  Grrr.  Need to store current position in QPoint cur.
				int dx = cur.x() - o.x();
				int dy = cur.y() - o.y();
				cur = o;

				if ( abs(dx) < guidemax*scale && abs(dy) < guidemax*scale ) {
					if ( newlyVisible ) {
						newlyVisible = false;
						psky.moveTo( o.x(), o.y() );
					} else {
						psky.lineTo( o.x(), o.y() );
					}
				} else {
					psky.moveTo( o.x(), o.y() );
				}
			}
	}

		//connect the final segment
		int dx = psky.pos().x() - o1.x();
		int dy = psky.pos().y() - o1.y();
		if ( abs(dx) < guidemax*scale && abs(dy) < guidemax*scale ) {
			psky.lineTo( o1.x(), o1.y() );
		} else {
			psky.moveTo( o1.x(), o1.y() );
		}
	}

	//next, the meridians
	for ( double RA=0.; RA<24.; RA += 2. ) {
		bool newlyVisible = false;
		SkyPoint *sp1 = new SkyPoint( RA, -90. );
		if ( Options::useAltAz() ) sp1->EquatorialToHorizontal( map->data()->LST, map->data()->geo()->lat() );
		QPoint o = map->getXY( sp1, Options::useAltAz(), Options::useRefraction(), scale );
		cur = o;
		psky.moveTo( o.x(), o.y() );

		double dDec = 1.;
		for ( double Dec=-89.; Dec<=90.; Dec+=dDec ) {
			sp1->set( RA, Dec );
			if ( Options::useAltAz() ) sp1->EquatorialToHorizontal( map->data()->LST, map->data()->geo()->lat() );

			if ( map->checkVisibility( sp1, guideFOV, guideXRange ) ) {
				o = map->getXY( sp1, Options::useAltAz(), Options::useRefraction(), scale );

				//When drawing on the printer, the psky.pos() point does NOT get updated
				//when lineTo or moveTo are called.  Grrr.  Need to store current position in QPoint cur.
				int dx = cur.x() - o.x();
				int dy = cur.y() - o.y();
				cur = o;

				if ( abs(dx) < guidemax*scale && abs(dy) < guidemax*scale ) {
					if ( newlyVisible ) {
						newlyVisible = false;
						psky.moveTo( o.x(), o.y() );
					} else {
						psky.lineTo( o.x(), o.y() );
					}
				} else {
					psky.moveTo( o.x(), o.y() );
				}
			}
		}
		delete sp1;  // avoid memory leak
	}
}
