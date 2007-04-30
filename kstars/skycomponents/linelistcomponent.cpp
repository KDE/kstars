/***************************************************************************
                          linelistcomponent.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 2006/11/01
    copyright            : (C) 2006 by Jason Harris
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

#include "linelistcomponent.h"

#include <QtAlgorithms>
#include <QPainter>

#include "skyline.h" 
#include "kstars.h"
#include "kstarsdata.h"
#include "skymap.h"
#include "dms.h"

LineListComponent::LineListComponent( SkyComponent *parent, bool (*visibleMethod)() )
	: SkyComponent( parent, visibleMethod ), LabelPosition( NoLabel ), 
		Label( QString() )
{
}

LineListComponent::~LineListComponent()
{
	qDeleteAll( m_LineList );
}

void LineListComponent::update( KStarsData *data, KSNumbers *num )
{
	if ( visible() ) {
		foreach ( SkyLine *sl, lineList() ) {
			sl->update( data, num );
		}
	}
}

void LineListComponent::draw( KStars *ks, QPainter &psky, double scale ) {
	if ( ! visible() ) return;

	SkyMap *map = ks->map();
	KStarsData *data = ks->data();

	psky.setPen( pen() );

	QLineF segment;
	QLineF segLeft = QLineF();   //the segment that intersects the left edge
	QLineF segRight = QLineF();   //the segment that intersects the right edge
	QLineF segTop = QLineF();    //the segment that intersects the top edge
	QLineF segBottom = QLineF(); //the segment that intersects the bottom edge

	foreach ( SkyLine *sl, lineList() ) {
		if ( map->checkVisibility( sl ) ) {
			segment = map->toScreen( sl, scale );

			if ( ! segment.isNull() ) { 
				psky.drawLine( segment );
				
				if ( LabelPosition != NoLabel ) {
					//Identify segLeft, the segment which brackets x=20.
					if ( ( segment.x1() >= 20.0 && segment.x2() < 20.0 ) ||
							 ( segment.x2() >= 20.0 && segment.x1() < 20.0 ) )
						segLeft = segment;

					//Identify segRight, the segment which brackets x=width-60.
					if ( ( segment.x1() >= map->width()-60.0 && segment.x2() < map->width()-60.0 ) ||
							 ( segment.x2() >= map->width()-60.0 && segment.x1() < map->width()-60.0 ) )
						segRight = segment;

					//Identify segTop, the segment which brackets y=10.
					if ( ( segment.y1() >= 20.0 && segment.y2() < 20.0 ) ||
							 ( segment.y2() >= 20.0 && segment.y1() < 20.0 ) ) 
						segTop = segment;

					//Identify segBottom, the segment which brackets y=height-30.
					if ( ( segment.y1() >= map->height()-30.0 && segment.y2() < map->height()-30.0 ) ||
							 ( segment.y2() >= map->height()-30.0 && segment.y1() < map->height()-30.0 ) )
						segBottom = segment;
				}
			}
		}
	}

	if ( LabelPosition != NoLabel ) {
		//Draw the label at segLeft or segRight.  If this segment is undefined,
		//choose segTop or segBottom, wheichever is further left or right
		segment = segLeft;
		double tx = 20.0;
		double ty = segment.y1() - 5.0;
		if ( LabelPosition == RightEdgeLabel ) {
			segment = segRight;
			tx = map->width() - 60.0;
			ty = segment.y1() - 5.0;
		}
		
		if ( segment.isNull() ) {
			if ( segTop.isNull() && segBottom.isNull() ) //No segments found for label
				return;
			
			if ( ! segTop.isNull() && ! segBottom.isNull() ) {
				segment = segBottom;
				if ( ( LabelPosition == LeftEdgeLabel && segTop.x1() < segBottom.x1() ) ||
						 ( LabelPosition == RightEdgeLabel && segTop.x1() > segBottom.x1() ) )
					segment = segTop;
				
			} else if ( ! segTop.isNull() ) {
				segment = segTop;
			} else if ( ! segBottom.isNull() ) {
				segment = segBottom;
			}

			tx = segment.x1();
			ty = segment.y1() - 5.0;
		}

		double sx = double( segment.x1() - segment.x2() );
		double sy = double( segment.y1() - segment.y2() );
		double angle = atan2( sy, sx )*180.0/dms::PI;
		if ( sx < 0.0 ) angle -= 180.0;

		psky.save();
		psky.translate( tx, ty );
		
		psky.rotate( double( angle ) );  //rotate the coordinate system
		psky.drawText( 0.0, 0.0, Label );
		psky.restore(); //reset coordinate system
	}
}
