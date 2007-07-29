/***************************************************************************
                          skymesh.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 200-07-03
    copyright            : (C) 2007 by James B. Bowlin
    email                : bowlin@mindspring.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <QHash>
#include <QPolygonF>
#include <QPointF>

#include "skypoint.h"
#include "skymesh.h"

// these are just for the draw routine:
#include <QPainter>
#include "kstars.h"
#include "skymap.h"
#include "kstarsdata.h"


SkyMesh::SkyMesh(int level) : HTMesh(level, level, NUM_BUF), m_drawID(0), callCnt(0)
{
    errLimit = HTMesh::size() / 4;
}

void SkyMesh::aperture(SkyPoint *p, double radius, BufNum bufNum)
{
    //FIXME: -jbb add reverse-precession here.

    if (radius > 90.0) radius = 90.0;
    HTMesh::intersect( p->ra()->Degrees(), p->dec()->Degrees(), radius, bufNum);
    m_drawID++;
}

Trixel SkyMesh::index(SkyPoint *p)
{
    return HTMesh::index( p->ra()->Degrees(), p->dec()->Degrees() );
}

void SkyMesh::index(SkyPoint *p, double radius)
{
    HTMesh::intersect( p->ra()->Degrees(), p->dec()->Degrees(), radius);
}

void SkyMesh::index( SkyPoint* p1, SkyPoint* p2 )
{
    HTMesh::intersect( p1->ra()->Degrees(), p1->dec()->Degrees(),
                        p2->ra()->Degrees(), p2->dec()->Degrees() );
}

void SkyMesh::index( SkyPoint* p1, SkyPoint* p2, SkyPoint* p3 )
{
    HTMesh::intersect( p1->ra()->Degrees(), p1->dec()->Degrees(),
                        p2->ra()->Degrees(), p2->dec()->Degrees(),
                        p3->ra()->Degrees(), p3->dec()->Degrees() );
}

void SkyMesh::index( SkyPoint* p1, SkyPoint* p2, SkyPoint* p3, SkyPoint* p4 )
{
    HTMesh::intersect( p1->ra()->Degrees(), p1->dec()->Degrees(),
                        p2->ra()->Degrees(), p2->dec()->Degrees(),
                        p3->ra()->Degrees(), p3->dec()->Degrees(),
                        p4->ra()->Degrees(), p4->dec()->Degrees() );
}

void SkyMesh::index( const QPointF &p1, const QPointF &p2, const QPointF &p3 )
{
    HTMesh::intersect( p1.x() * 15.0, p1.y(),
                       p2.x() * 15.0, p2.y(),
                       p3.x() * 15.0, p3.y() );
}

void SkyMesh::index( const QPointF &p1, const QPointF &p2, const QPointF &p3, const QPointF &p4 )
{
    HTMesh::intersect( p1.x() * 15.0, p1.y(),
                       p2.x() * 15.0, p2.y(),
                       p3.x() * 15.0, p3.y(),
                       p4.x() * 15.0, p4.y() );
}

const IndexHash& SkyMesh::indexLine( SkyList* points, int debug )
{
    return indexLine( points, NULL, debug);
}


const IndexHash& SkyMesh::indexLine( SkyList* points, IndexHash* skip, int debug )
{
    SkyPoint *pThis, *pLast;

    indexHash.clear();

    if ( points->size() == 0 ) return indexHash;

    pLast = points->at( 0 );
    for ( int i=1 ; i < points->size() ; i++ ) {
        pThis = points->at( i );

        if (skip != NULL && skip->contains( i ) ) {
            pLast = pThis;
            continue;
        }

        index( pThis, pLast );
        MeshIterator region( this );

        if ( region.size() > errLimit ) {
            printf("\nSkyMesh::indexLine: too many trixels: %d\n", region.size() );
            printf("    ra1  = %f;\n", pThis->ra()->Degrees());
            printf("    ra2  = %f;\n", pLast->ra()->Degrees());
            printf("    dec1 = %f;\n", pThis->dec()->Degrees());
            printf("    dec2 = %f;\n", pLast->dec()->Degrees());
            HTMesh::setDebug( 10 );
            index( pThis, pLast );
            HTMesh::setDebug ( 0 );

        }

        // This was used to track down a bug in my HTMesh code. The bug was caught
        // and fixed but I've left this debugging code in for now.  -jbb

        else {
            while ( region.hasNext() ) {
                indexHash[ region.next() ] = true;
            }
        }
        pLast = pThis;
    }
    return indexHash;
}


// ----- Create HTMesh Index for Polygons -----
// Create (mostly) 4-point polygons that cover the mw polygon and
// all share the same first vertex.  Use indexHash to eliminate
// the many duplicate indices that are generated with this procedure.
// There are probably faster and better ways to do this.

const IndexHash& SkyMesh::indexPoly( SkyList *points, int debug )
{

    indexHash.clear();

    if (points->size() < 3) return indexHash;

    SkyPoint* startP = points->first();

    int end = points->size() - 2;     // 1) size - 1  -> last index,
                                      // 2) minimum of 2 points
    callCnt++;

    for( int p = 1; p <= end; p+= 2 ) {

        /**
        if (false) {
            endP = ( p == end) ? points->at(p+1) : points->at(p+2);
            if ( startRa == endP->ra()->Degrees() &&
                 startDec == endP->dec()->Degrees() ) continue;
        }

        if ( false ) {
            printf("%d: poly %3d of %3d:\n", callCnt,  ++cnt, end );
            printf("    ra1 = %f;\n", startP->ra()->Degrees());
            printf("    ra2 = %f;\n", points->at(p)->ra()->Degrees());
            printf("    ra3 = %f;\n", points->at(p+1)->ra()->Degrees());
            if ( p < end )
                printf("    ra4 = %f;\n", points->at(p+2)->ra()->Degrees());

            printf("    dec1 = %f;\n", startP->dec()->Degrees());
            printf("    dec2 = %f;\n", points->at(p)->dec()->Degrees());
            printf("    dec3 = %f;\n", points->at(p+1)->dec()->Degrees());
            if ( p < end )
                printf("    dec4 = %f;\n", points->at(p+2)->dec()->Degrees());

            printf("\n");
        }
        **/

        if ( p == end ) {
            index( startP, points->at(p), points->at(p+1) );
        }
        else {
            index( startP, points->at(p), points->at(p+1), points->at(p+2) );
        }

        MeshIterator region( this );

        if ( region.size() > errLimit ) {
            printf("\nSkyMesh::indexPoly: too many trixels: %d\n", region.size() );

            printf("    ra1 = %f;\n", startP->ra()->Degrees());
            printf("    ra2 = %f;\n", points->at(p)->ra()->Degrees());
            printf("    ra3 = %f;\n", points->at(p+1)->ra()->Degrees());
            if ( p < end )
                printf("    ra4 = %f;\n", points->at(p+2)->ra()->Degrees());

            printf("    dec1 = %f;\n", startP->dec()->Degrees());
            printf("    dec2 = %f;\n", points->at(p)->dec()->Degrees());
            printf("    dec3 = %f;\n", points->at(p+1)->dec()->Degrees());
            if ( p < end )
                printf("    dec4 = %f;\n", points->at(p+2)->dec()->Degrees());

            printf("\n");

        }
        while ( region.hasNext() ) {
            indexHash[ region.next() ] = true;
        }
    }
    return indexHash;
}

const IndexHash& SkyMesh::indexPoly( const QPolygonF &points, int debug )
{
    indexHash.clear();

    if (points.size() < 3) return indexHash;

    const QPointF startP = points.first();

    int end = points.size() - 2;     // 1) size - 1  -> last index,
                                      // 2) minimum of 2 points
    callCnt++;

    for( int p = 1; p <= end; p+= 2 ) {

        if ( p == end ) {
            index( startP, points.at(p), points.at(p+1) );
        }
        else {
            index( startP, points.at(p), points.at(p+1), points.at(p+2) );
        }

        MeshIterator region( this );

        if ( region.size() > errLimit ) {
            printf("\nSkyMesh::indexPoly: too many trixels: %d\n", region.size() );

            printf("    ra1 = %f;\n", startP.x() );
            printf("    ra2 = %f;\n", points.at(p).x() );
            printf("    ra3 = %f;\n", points.at(p+1).x()) ;
            if ( p < end )
                printf("    ra4 = %f;\n", points.at(p+2).x() );

            printf("    dec1 = %f;\n", startP.y() );
            printf("    dec2 = %f;\n", points.at(p).y() );
            printf("    dec3 = %f;\n", points.at(p+1).y() );
            if ( p < end )
                printf("    dec4 = %f;\n", points.at(p+2).y());

            printf("\n");

        }
        while ( region.hasNext() ) {
            indexHash[ region.next() ] = true;
        }
    }
    return indexHash;
}

void SkyMesh::draw(KStars *kstars, QPainter& psky, double scale, BufNum bufNum)
{
    SkyMap*     map  = kstars->map();
    KStarsData* data = kstars->data();

    //QPainter psky;
    //psky.begin( map );

    double r1, d1, r2, d2, r3, d3;

    MeshIterator region( this, bufNum );
    while ( region.hasNext() ) {
        Trixel trixel = region.next();
        vertices( trixel, &r1, &d1, &r2, &d2, &r3, &d3 );
        SkyPoint s1( r1 / 15.0, d1 );
        SkyPoint s2( r2 / 15.0, d2 );
        SkyPoint s3( r3 / 15.0, d3 );
        s1.EquatorialToHorizontal( data->lst(), data->geo()->lat() );
        s2.EquatorialToHorizontal( data->lst(), data->geo()->lat() );
        s3.EquatorialToHorizontal( data->lst(), data->geo()->lat() );
        QPointF q1 = map->toScreen( &s1, scale );
        QPointF q2 = map->toScreen( &s2, scale );
        QPointF q3 = map->toScreen( &s3, scale );
        psky.drawLine( q1, q2 );
        psky.drawLine( q2, q3 );
        psky.drawLine( q3, q1 );
    }
}


