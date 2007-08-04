/***************************************************************************
                 constellationboundary.cpp -  K Desktop Planetarium
                             -------------------
    begin                : 25 Oct. 2005
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

#include <stdio.h>

#include <QPen>
#include <QPainter>
#include <kstandarddirs.h>

#include <kdebug.h>
#include <klocale.h>

#include "Options.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "ksutils.h"
#include "skyobject.h"
#include "ksfilereader.h"

#include "typedef.h"
#include "linelist.h"
#include "polylist.h"
#include "linelistindex.h"
#include "constellationboundary.h"
#include "constellationboundarypoly.h"

#include "skymesh.h"

ConstellationBoundary::ConstellationBoundary( SkyComponent *parent )
  : LineListIndex( parent, "Constellation Boundaries" )
{
    m_Boundary = new ConstellationBoundaryPoly( parent );
}

// FIXME: -jbb: need to update this information.
// Constellation boundary data is stored in a series of *.cbound files, one per
// constellation.  Each file contains the list of RA/Dec points along the
// constellation's border, and a flag indicating whether the segment is
// duplicated in another constellation.  (Actually all segments have a
// duplicate somewhere...the choice of calling one the duplicate is entirely
// arbitrary).
//
// We store the boundary data in a QHash of QPolygonF's (for fast determination
// of whether a SkyPoint is enclosed, and for drawing a single boundary to the
// screen).  We also store the non-duplicate segments in the Component's native
// list of SkyLines (for fast drawing of all boundaries at once).

void ConstellationBoundary::init( KStarsData *data ) {

    char* fname = "cbounds.dat";
    int flag;
    double ra, dec, lastRa, lastDec;
	LineList *lineList = 0;
    PolyList *polyList = 0;
    bool ok;

    intro();

	KSFileReader fileReader;
    if ( ! fileReader.open( fname ) ) return;

    fileReader.setProgress( data, 
        QString("Loading Consellation Boundaries (%1%)"), 13124, 20, 100 );

    lastRa = lastDec = -1000.0;

    while ( fileReader.hasMoreLines() ) {
		QString line = fileReader.readLine();
        fileReader.showProgress();

        if ( line.at( 0 ) == '#' ) continue;     // ignore comments
        if ( line.at( 0 ) == ':' ) {             // :NAM line

            if ( lineList ) appendLine( lineList );
            lineList = 0; //new LineList();

            if ( polyList ) m_Boundary->appendPoly( polyList );
            QString cName = line.mid(1, 3); 
            polyList = new PolyList( cName );
            
            //kDebug() << QString(":%1\n").arg( polyList->name() );

            continue;
        }

        // read in the data from the line
		ra = line.mid(  0, 12 ).toDouble( &ok );
		if ( ok ) dec = line.mid( 13, 12 ).toDouble( &ok );
        if ( ok ) flag = line.mid( 26,  1 ).toInt(&ok);
        if ( !ok ) {
            fprintf(stderr, "%s: conversion error on line: %d\n",
                    fname, fileReader.lineNumber() );
            continue;
        }

        //fprintf(stderr, "%12.7f %12.7f %d\n", ra, dec, flag);

        if ( ra == lastRa && dec == lastDec ) {
            fprintf(stderr, "%s: tossing dupe on line %4d: (%f, %f)\n", 
                    fname, fileReader.lineNumber(), ra, dec);
            continue;
        }

        // always add the point to the boundary (and toss dupes)
        polyList->append( QPointF( ra, dec ) );
        if ( ra < 0 ) polyList->wrapRA( true );
        
        if ( flag ) {

            if ( ! lineList ) lineList = new LineList();

            SkyPoint* point = new SkyPoint( ra, dec );
            point->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
            lineList->points()->append( point );
            lastRa  = ra;
            lastDec = dec;
        }
        else {
            if ( lineList ) appendLine( lineList );
            lineList = 0;
            lastRa = lastDec = -1000.0;
        }
    }

    if ( lineList ) appendLine( lineList );
    if ( polyList ) m_Boundary->appendPoly( polyList );

    summary();
    m_Boundary->summary();

}

// Don't precess the coordinate grid
void ConstellationBoundary::JITupdate( KStarsData *data, LineList* lineList )
{
    lineList->updateID = data->updateID();
    SkyList* points = lineList->points();
    for (int i = 0; i < points->size(); i++ ) {
        points->at( i )->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
    }
}

bool ConstellationBoundary::selected()
{
    return Options::showCBounds();
}

void ConstellationBoundary::preDraw( KStars *kstars, QPainter &psky )
{
    QColor color = kstars->data()->colorScheme()->colorNamed( "CBoundColor" );
    //color = QColor("red");
    psky.setPen( QPen( QBrush( color ), 1, Qt::SolidLine ) );
}

