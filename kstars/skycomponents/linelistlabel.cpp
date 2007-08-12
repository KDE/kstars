/***************************************************************************
					linelistlabel.cpp -  K Desktop Planetarium
							 -------------------
	begin				: 2007-08-08
	copyright			: (C) 2007 James B. Bowlin
	email				: bowlin@mindspring.com
 ***************************************************************************/

/***************************************************************************
 *																		 *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or	 *
 *   (at your option) any later version.								   *
 *																		 *
 ***************************************************************************/


#include "Options.h"
#include "skypoint.h"
#include "kstars.h"
#include "skymap.h"
#include "kstarsdata.h"
#include "linelistlabel.h"
#include "skylabeler.h"
#include "linelist.h"

LineListLabel::LineListLabel( const QString& text ) 
	: m_text( text )
{
	m_skyLabeler = SkyLabeler::Instance();

	// prevent a crash if drawLabel() is called before reset()
	for ( int i = 0; i < 4; i++ ) {
		m_labList[i] = 0;
		m_labIndex[i] = 0;
	}
}

void LineListLabel::reset( QPainter &psky )
{
	// These are the indices of the farthest left point, farthest right point,
	// etc.  The are data members so the drawLabels() routine can use them.
	// Zero indicates an index that was never set and is considered invalid
	// inside of drawLabels().
	for ( int i = 0; i < 4; i++ ) {
		m_labList[i] = 0;
		m_labIndex[i] = 0;
	}

	// These are used to keep track of the element that is farthest left,
	// farthest right, etc.
	m_farLeft  = 100000.0;
	m_farRight = 0.0;
	m_farTop   = 100000.0;
	m_farBot   = 0.0;

	// We don't draw the label here but we need the proper font in order to set
	// the margins correctly.  Since psky no contains the zoom dependent font as
	// its default font, we need to play the little dance below.
	QFontMetricsF fm = skyLabeler()->fontMetrics();
	float height     = fm.height();
	float width      = fm.width( m_text );
	float sideMargin = fm.width("MM") + width / 2.0;

	// Create the margins within which it is okay to draw the label
	m_marginRight = psky.window().width() - sideMargin;
	m_marginLeft  = sideMargin;
	m_marginTop   = height;
	m_marginBot   = psky.window().height() - 2.0 * height;
}

void LineListLabel::updateLabelCandidates( qreal x, qreal y, LineList* lineList, int i )
{
	if ( i < 1 ) return;

	if ( x < m_marginLeft || x > m_marginRight ||
		 y < m_marginTop  || y > m_marginBot ) return;

	if ( x < m_farLeft ) {
		m_labIndex[LeftCandidate] = i;
		m_labList[LeftCandidate]  = lineList;
		m_farLeft = x;
	}
	if ( x > m_farRight ) {
		m_labIndex[RightCandidate] = i;
		m_labList[RightCandidate]  = lineList;
		m_farRight = x;
	}
	if (  y > m_farBot ) {
		m_labIndex[BotCandidate] = i;
		m_labList[BotCandidate]  = lineList;
		m_farBot = x;
	}
	if ( y < m_farTop ) {
		m_labIndex[TopCandidate] = i;
		m_labList[TopCandidate]  = lineList;
		m_farTop = x;
	}
}


void LineListLabel::draw( KStars* kstars, QPainter& psky, double scale )
{

	SkyMap *map = kstars->map();

	double comfyAngle = 40.0;  // the first valid candidate with an angle
							   // smaller than this gets displayed.  If you set
							   // this to > 90. then the first valid candidate
							   // will be displayed, regardless of angle.

	// We store info about the four candidate points in arrays to make several
	// of the steps easier, particularly choosing the valid candidate with the
	// smallest angle from the horizontal.

	int		  idx[4];								  // index of candidate
	LineList* list[4];								  // LineList of candidate
	double	  a[4] = { 360.0, 360.0, 360.0, 360.0 };  // angle, default to large value
	QPointF	  o[4];								      // candidate point
	bool	  okay[4] = { true, true, true, true };	  // flag  candidate false if it
													  // overlaps a previous label.

	// We no longer adjust the order but if we were to it would be here
 	static int Order[4]  = 
		 { RightCandidate, LeftCandidate, BotCandidate, TopCandidate }; 

	for ( int j = 0; j < 4; j++ ) {
		idx[j]	= m_labIndex[ Order[ j ] ];
		list[j] = m_labList[  Order[ j ] ];
	}

	// Make sure start with a valid candidate
	int first = 0;
	for ( ; first < 4; first++ ) {
		if ( idx[first] ) break;
	}

	// return if there are no valid candidates
	if ( first >= 4 ) return;


	// Try the points in order and print the label if we can draw it at
	// a comfortable angle for viewing;
	for ( int j = first; j < 4; j++ ) {
		o[j] = angleAt( map, list[j], idx[j], &a[j], scale );
		if ( fabs( a[j] ) > comfyAngle ) continue;
		if ( skyLabeler()->drawLabel( psky, o[j], m_text, a[j] ) )
			return;
		okay[j] = false;
	}

	//--- No angle was comfy so pick the one with the smallest angle ---

	// Index of the index/angle/point that gets displayed	
	int best = first;

	// find first valid candidate that does not overlap existing labels
	for ( ; best < 4; best++ ) {
		if ( idx[best] && okay[best] ) break;
	}

	// return if all candiates either overlap or are invalid
	if ( best >= 4 ) return;

	// find the valid non-overlap candidate with the smallest angle
	for ( int j = best + 1; j < 4; j++ ) {
		if ( idx[j] && okay[j] && fabs(a[j]) < fabs(a[best]) ) best = j;
	}

	skyLabeler()->drawLabel( psky, o[best], m_text, a[best] );
}


QPointF LineListLabel::angleAt( SkyMap* map, LineList* list, int i, double *angle, double scale )
{
	SkyPoint* pThis = list->at( i );
	SkyPoint* pLast = list->at( i - 1 );

	QPointF oThis = map->toScreen( pThis, scale, false );
	QPointF oLast = map->toScreen( pLast, scale, false );

	double sx = double( oThis.x() - oLast.x() );
	double sy = double( oThis.y() - oLast.y() );

	*angle = atan2( sy, sx ) * 180.0 / dms::PI;

	// Never draw the label upside down
	if ( *angle < -90.0 ) *angle += 180.0;
	if ( *angle >  90.0 ) *angle -= 180.0;

	return oThis;
}


