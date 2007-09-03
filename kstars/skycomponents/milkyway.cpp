/***************************************************************************
                          milkyway.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 12 Nov. 2005
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

#include <QList>
#include <QPointF>
#include <QPolygonF>
#include <QPainter>

#include <klocale.h>

#include "kstars.h"
#include "kstarsdata.h"
#include "skymap.h"
#include "skypoint.h"
#include "dms.h"
#include "Options.h"
#include "ksfilereader.h"

#include "skymesh.h"

#include "milkyway.h"


MilkyWay::MilkyWay( SkyComponent *parent ) : 
    SkipListIndex( parent, i18n("Milky Way") )
{}

void MilkyWay::init( KStarsData *data )
{
    intro();

    char* fname = "milkyway.dat";
	QString line;
    double ra, dec, lastRa, lastDec;
	SkipList *skipList = 0;
    bool ok;
    int iSkip = 0;

    lastRa = lastDec = -1000.0;

	KSFileReader fileReader;
    if ( ! fileReader.open( fname ) ) return;

    fileReader.setProgress( i18n("Loading Milky Way"), 2136, 5 );

	while ( fileReader.hasMoreLines() ) {
		line = fileReader.readLine();

		fileReader.showProgress();

        QChar firstChar = line.at( 0 );
        if ( firstChar == '#' ) continue; 

        ra = line.mid( 2, 8 ).toDouble(&ok);        
		if ( ok ) dec = line.mid( 11, 8 ).toDouble(&ok);
        if ( !ok ) {
            fprintf(stderr, "%s: conversion error on line: %d\n",
                    fname, fileReader.lineNumber());
            continue;
        }

		if ( firstChar == 'M' )  {
			if (  skipList )  appendBoth( skipList );
			skipList = 0;
            iSkip = 0;
            lastRa = lastDec = -1000.0;
		}

        if ( ! skipList ) skipList = new SkipList();

        if ( ra == lastRa && dec == lastDec ) {
            fprintf(stderr, "%s: tossing dupe on line %4d: (%f, %f)\n",
                    fname, fileReader.lineNumber(), ra, dec);
            continue;
        }

		skipList->append( new SkyPoint(ra, dec) );
        lastRa = ra;
        lastDec = dec;
		if ( firstChar == 'S' ) skipList->setSkip( iSkip );
        iSkip++;
	}
    if ( skipList ) appendBoth( skipList );

    summary();
        //printf("Done.\n");
}

bool MilkyWay::selected() 
{
    return Options::showMilkyWay() &&
		! ( Options::hideOnSlew() && Options::hideMilkyWay() && SkyMap::IsSlewing() );
}

void MilkyWay::draw(KStars *kstars, QPainter& psky, double scale)
{
	if ( !selected() ) return;

    QColor color =  QColor( kstars->data()->colorScheme()->colorNamed( "MWColor" ) );

	psky.setPen( QPen( color, 3, Qt::SolidLine ) );
    psky.setBrush( QBrush( color ) );

	// Uncomment these two lines to get more visible images for debugging.  -jbb
	//
	//psky.setPen( QPen( QColor( "red" ), 1, Qt::SolidLine ) );
	//psky.setBrush( QBrush( QColor("green"  ) ) );

    if ( Options::fillMilkyWay() ) {
        drawFilled( kstars, psky, scale );
    }
    else {
        drawLines( kstars, psky, scale );        
    }
}

