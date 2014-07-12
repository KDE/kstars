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

#include "constellationboundarylines.h"

#include <cstdio>

#include <QPen>


#include <QDebug>
#include <KLocalizedString>

#include "Options.h"
#include "kstarsdata.h"
#include "skymap.h"
#include "skyobjects/skyobject.h"
#include "ksfilereader.h"

#include "typedef.h"
#include "linelist.h"
#include "polylist.h"
#include "linelistindex.h"
#include "skycomponents/skymapcomposite.h"

#include "skymesh.h"

#include "skypainter.h"

ConstellationBoundaryLines::ConstellationBoundaryLines( SkyComposite *parent )
        : NoPrecessIndex( parent, xi18n("Constellation Boundaries") )
{
    m_skyMesh = SkyMesh::Instance();
    m_polyIndexCnt = 0;
    for(int i = 0; i < m_skyMesh->size(); i++) {
        m_polyIndex.append( new PolyListList() );
    }

    KStarsData *data = KStarsData::Instance();
    int verbose = 0;                  // -1 => create cbounds-$x.idx on stdout
    //  0 => normal
    const char* fname = "cbounds.dat";
    int flag;
    double ra, dec, lastRa, lastDec;
    LineList *lineList = 0;
    PolyList *polyList = 0;
    bool ok;

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

    fileReader.setProgress( xi18n("Loading Constellation Boundaries"), 13124, 10 );

    lastRa = lastDec = -1000.0;

    while ( fileReader.hasMoreLines() ) {
        QString line = fileReader.readLine();
        fileReader.showProgress();

        if ( line.at( 0 ) == '#' ) continue;     // ignore comments
        if ( line.at( 0 ) == ':' ) {             // :constellation line

            if ( lineList ) appendLine( lineList );
            lineList = 0;

            if ( polyList )
                appendPoly( polyList, idxFile, verbose );
            QString cName = line.mid(1);
            polyList = new PolyList( cName );
            if ( verbose == -1 ) printf(":\n");
            lastRa = lastDec = -1000.0;
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

        // By the time we come here, we should have polyList. Else we aren't doing good
        Q_ASSERT( polyList ); // Is this the right fix?

        polyList->append( QPointF( ra, dec ) );
        if ( ra < 0 )
            polyList->setWrapRA( true );

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

    if( lineList )
        appendLine( lineList );
    if( polyList )
        appendPoly( polyList, idxFile, verbose );
}

bool ConstellationBoundaryLines::selected()
{
    return Options::showCBounds() &&
           ! ( Options::hideOnSlew() && Options::hideCBounds() && SkyMap::IsSlewing() );
}

void ConstellationBoundaryLines::preDraw( SkyPainter* skyp )
{
    QColor color = KStarsData::Instance()->colorScheme()->colorNamed( "CBoundColor" );
    skyp->setPen( QPen( QBrush( color ), 1, Qt::SolidLine ) );
}

void ConstellationBoundaryLines::appendPoly( PolyList* polyList, KSFileReader* file, int debug)
{
    if ( ! file || debug == -1)
        return appendPoly( polyList, debug );

    while ( file->hasMoreLines() ) {
        QString line = file->readLine();
        if ( line.at( 0 ) == ':' ) return;
        Trixel trixel = line.toInt();
        m_polyIndex[ trixel ]->append( polyList );
    }
}

void ConstellationBoundaryLines::appendPoly( PolyList* polyList, int debug)
{
    if ( debug >= 0 && debug < m_skyMesh->debug() ) debug = m_skyMesh->debug();

    const IndexHash& indexHash = m_skyMesh->indexPoly( polyList->poly() );
    IndexHash::const_iterator iter = indexHash.constBegin();
    while ( iter != indexHash.constEnd() ) {
        Trixel trixel = iter.key();
        iter++;

        if ( debug == -1 ) printf("%d\n", trixel );

        m_polyIndex[ trixel ]->append( polyList );
    }

    if ( debug > 9 )
        printf("PolyList: %3d: %d\n", ++m_polyIndexCnt, indexHash.size() );
}

PolyList* ConstellationBoundaryLines::ContainingPoly( SkyPoint *p )
{
    //printf("called ContainingPoly(p)\n");

    // we save the pointers in a hash because most often there is only one
    // constellation and we can avoid doing the expensive boundary calculations
    // and just return it if we know it is unique.  We can avoid this minor
    // complication entirely if we use index(p) instead of aperture(p, r)
    // because index(p) always returns a single trixel index.

    QHash<PolyList*, bool> polyHash;
    QHash<PolyList*, bool>::const_iterator iter;

    //printf("\n");

    // the boundaries don't precess so we use index() not aperture()
    m_skyMesh->index( p, 1.0, IN_CONSTELL_BUF );
    MeshIterator region( m_skyMesh, IN_CONSTELL_BUF );
    while ( region.hasNext() ) {

        Trixel trixel = region.next();
        //printf("Trixel: %4d %s\n", trixel, m_skyMesh->indexToName( trixel ) );

        PolyListList *polyListList = m_polyIndex[ trixel ];

        //printf("    size: %d\n", polyListList->size() );

        for (int i = 0; i < polyListList->size(); i++) {
            PolyList* polyList = polyListList->at( i );
            polyHash.insert( polyList, true );
        }
    }

    iter = polyHash.constBegin();

    // Don't bother with boundaries if there is only one
    if ( polyHash.size() == 1 ) return iter.key();

    QPointF point( p->ra().Hours(), p->dec().Degrees() );
    QPointF wrapPoint( p->ra().Hours() - 24.0, p->dec().Degrees() );
    bool wrapRA = p->ra().Hours() > 12.0;

    while ( iter != polyHash.constEnd() ) {

        PolyList* polyList = iter.key();
        iter++;

        //qDebug() << QString("checking %1 boundary\n").arg( polyList->name() );

        const QPolygonF* poly = polyList->poly();
        if ( wrapRA && polyList->wrapRA() ) {
            if ( poly->containsPoint( wrapPoint, Qt::OddEvenFill ) )
                return polyList;
        }
        else {
            if ( poly->containsPoint( point, Qt::OddEvenFill ) )
                return polyList;
        }
    }

    return 0;
}


//-------------------------------------------------------------------
// The routines for providing public access to the boundary index
// start here.  (Some of them may not be needed (or working)).
//-------------------------------------------------------------------

QString ConstellationBoundaryLines::constellationName( SkyPoint *p )
{
    PolyList *polyList = ContainingPoly( p );
    if ( polyList ) {
        return ( Options::useLocalConstellNames() ?
                 i18nc( "Constellation name (optional)", polyList->name().toUpper().toLocal8Bit().data() ) :
                 polyList->name() );
    }
    return xi18n("Unknown");
}
