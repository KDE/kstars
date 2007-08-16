/***************************************************************************
                 constellationboundarylines.cpp -  K Desktop Planetarium
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
#include "skymap.h"
#include "ksutils.h"
#include "skyobject.h"
#include "ksfilereader.h"

#include "typedef.h"
#include "linelist.h"
#include "polylist.h"
#include "linelistindex.h"
#include "constellationboundarylines.h"
#include "constellationboundary.h"

#include "skymesh.h"

ConstellationBoundaryLines::ConstellationBoundaryLines( SkyComponent *parent )
  : NoPrecessIndex( parent, i18n("Constellation Boundaries") )
{
    ConstellationBoundary::Create( parent );
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

void ConstellationBoundaryLines::init( KStarsData *data ) {

	int verbose = 0;                  // -1 => create cbounds-$x.idx on stdout
	                                  //  0 => normal
    char* fname = "cbounds.dat";
    int flag;
    double ra, dec, lastRa, lastDec;
	LineList *lineList = 0;
    PolyList *polyList = 0;
    bool ok;

	ConstellationBoundary* boundaryPoly = ConstellationBoundary::Instance();

    intro();

	// Open the .idx file and skip past the first line
	KSFileReader idxReader, *idxFile = 0;
	QString idxFname = QString("cbounds-%1.idx").arg( SkyMesh::Instance()->level() );
	if ( idxReader.open( idxFname ) ) {
		idxReader.readLine();
		idxFile = &idxReader;
	}

	// now open the file that contains the points
	KSFileReader fileReader;
    if ( ! fileReader.open( fname ) ) return;

    fileReader.setProgress( data, 
            i18n("Loading Consellation Boundaries"), 13124, 10, 100 );

    lastRa = lastDec = -1000.0;

    while ( fileReader.hasMoreLines() ) {
		QString line = fileReader.readLine();
        fileReader.showProgress();

        if ( line.at( 0 ) == '#' ) continue;     // ignore comments
        if ( line.at( 0 ) == ':' ) {             // :constellation line

            if ( lineList ) appendLine( lineList );
            lineList = 0;

            if ( polyList ) boundaryPoly->appendPoly( polyList, idxFile, verbose );
            QString cName = line.mid(1, 3); 
            polyList = new PolyList( cName );
			if ( verbose == -1 ) printf(":\n");
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
            lineList->append( point );
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
    if ( polyList ) boundaryPoly->appendPoly( polyList, idxFile, verbose );

    summary();
    boundaryPoly->summary();
}

bool ConstellationBoundaryLines::selected()
{
    return Options::showCBounds() &&
		! ( Options::hideOnSlew() && Options::hideCBounds() && SkyMap::IsSlewing() );
}

void ConstellationBoundaryLines::preDraw( KStars *kstars, QPainter &psky )
{
    QColor color = kstars->data()->colorScheme()->colorNamed( "CBoundColor" );
    //color = QColor("red");
    psky.setPen( QPen( QBrush( color ), 1, Qt::SolidLine ) );
}

