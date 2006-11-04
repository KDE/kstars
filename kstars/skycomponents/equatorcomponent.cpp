/***************************************************************************
                          equatorcomponent.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 10 Sept. 2005
    copyright            : (C) 2005 by Jason Harris
    email                : kstars@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "equatorcomponent.h"

#include <QList>
#include <QPoint>
#include <QBrush>

#include "ksnumbers.h"
#include "kstarsdata.h"
#include "skypoint.h"
#include "skyline.h"

EquatorComponent::EquatorComponent(SkyComponent *parent, bool (*visibleMethod)()) : LineListComponent(parent, visibleMethod)
{
}

EquatorComponent::~EquatorComponent()
{
}

void EquatorComponent::init(KStarsData *data)
{
	emitProgressText( i18n("Creating equator" ) );

	setLabel( i18n("Equator") );
	setLabelPosition( LineListComponent::LeftEdgeLabel );
	setPen( QPen( QBrush( data->colorScheme()->colorNamed( "EqColor" ) ), 
										 1, Qt::SolidLine ) );

	SkyPoint o1( 0.0, 0.0 );
	for ( unsigned int i=1; i<=NCIRCLE; ++i ) {
		SkyPoint o2( i*24./NCIRCLE, 0.0 );
		SkyLine *sl = new SkyLine( o1, o2 );
		sl->update( data ); //calls EquatorialToHorizontal() on endpoints
		lineList().append( sl );
		o1 = o2;
	}
}

/*
void EquatorComponent::draw( KStars *ks, QPainter &psky, double scale ) {
	if ( ! visible() ) return;

	SkyMap *map = ks->map();
	KStarsData *data = ks->data();

	psky.setPen( QPen( QColor( data->colorScheme()->colorNamed( "EqColor" ) ), 
										 1, Qt::SolidLine ) );

	QLineF segment;
	QLineF segLeft = QLineF();   //the segment that intersects the left edge
	QLineF segTop = QLineF();    //the segment that intersects the top edge
	QLineF segBottom = QLineF(); //the segment that intersects the bottom edge

	foreach ( SkyLine *sl, lineList() ) {
		if ( map->checkVisibility( sl ) ) {
			segment = map->toScreen( sl, scale );

			if ( ! segment.isNull() ) { 
				psky.drawLine( segment );
				
				//Identify segLeft (it's the one with a point that has x() == 0.0)
				if ( segment.x1() == 0.0 || segment.x2() == 0.0 )
					segLeft = segment;

				//Identify segTop (it's the one with a point that has y() == 0.0)
				if ( segment.y1() == 0.0 || segment.y2() == 0.0 )
					segTop = segment;

				//Identify segBottom (it's the one with a point that 
				//has y() == map->height() )
				if ( segment.y1() == map->height() || segment.y2() == map->height() )
					segBottom = segment;
			}
		}
	}

	//Draw the Equator label at segLeft.  If segLeft is undefined, choose 
	//segTop or segBottom, wheichever is further left
	if ( segLeft.isNull() ) {
		if ( segTop.isNull() && segBottom.isNull() ) //No segments found for label
			return;

		if ( ! segTop.isNull() && ! segBottom.isNull() ) {
			if ( segTop.x1() < segBottom.x1() )
				segLeft = segTop;
			else
				segLeft = segBottom;
		} else if ( ! segTop.isNull() ) {
			segLeft = segTop;
		} else if ( ! segBottom.isNull() ) {
			segLeft = segBottom;
		}
	}

	double sx = double( segLeft.x1() - segLeft.x2() );
	double sy = double( segLeft.y1() - segLeft.y2() );
	double angle = atan2( sy, sx )*180.0/dms::PI;

	psky.save();
	psky.translate( 20.0, segLeft.y1() - 5.0 );
	
	psky.rotate( double( angle ) );  //rotate the coordinate system
	psky.drawText( 0, 0, i18n( "Equator" ) );
	psky.restore(); //reset coordinate system
}
*/
