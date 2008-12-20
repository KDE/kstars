/***************************************************************************
                 polylistindex.cpp -  K Desktop Planetarium
                             -------------------
    begin                : 2007-07-17
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

#include "polylistindex.h"

#include <stdio.h>

#include <kdebug.h>
#include <klocale.h>
#include <QPolygonF>

#include "Options.h"
#include "kstars.h"
#include "ksutils.h"
#include "skyobject.h"
#include "ksfilereader.h"

#include "polylist.h"

#include "skymesh.h"

PolyListIndex::PolyListIndex( SkyComponent *parent )
        : SkyComposite( parent)
{
    m_skyMesh = SkyMesh::Instance();
    m_polyIndexCnt = 0;
    for (int i = 0; i < skyMesh()->size(); i++) {
        m_polyIndex.append( new PolyListList() );
    }
}

void PolyListIndex::appendPoly( PolyList* polyList, KSFileReader* file, int debug)
{
    if ( ! file || debug == -1)
        return appendPoly( polyList, debug );

    m_nameHash.insert( polyList->name(), polyList );
    if( !polyList->localizedName().isEmpty() )
        m_nameHash.insert( polyList->localizedName(), polyList );

    while ( file->hasMoreLines() ) {
        QString line = file->readLine();
        if ( line.at( 0 ) == ':' ) return;
        Trixel trixel = line.toInt();
        m_polyIndex[ trixel ]->append( polyList );
    }
}

void PolyListIndex::appendPoly( PolyList* polyList, int debug)
{
    m_nameHash.insert( polyList->name(), polyList );
    if( !polyList->localizedName().isEmpty() )
        m_nameHash.insert( polyList->localizedName(), polyList );

    if ( debug >= 0 && debug < skyMesh()->debug() ) debug = skyMesh()->debug();

    const IndexHash& indexHash = skyMesh()->indexPoly( polyList->poly() );
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


void PolyListIndex::summary()
{
    if ( skyMesh()->debug() < 2 ) return;

    int total = skyMesh()->size();
    int polySize = m_polyIndex.size();

    /**
    for ( int i = 0; i < polySize; i++ ) {
    	PolyListList* listList = m_polyIndex.at( i );
    	printf("%4d: %d\n", i, listList->size() );
    }
    **/

    if ( polySize > 0 )
        printf("%4d out of %4d trixels in poly index %3d%%\n",
               polySize, total, 100 * polySize / total );

}

PolyList* PolyListIndex::ContainingPoly( SkyPoint *p )
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
    skyMesh()->index( p, 1.0, IN_CONSTELL_BUF );
    MeshIterator region( skyMesh(), IN_CONSTELL_BUF );
    while ( region.hasNext() ) {

        Trixel trixel = region.next();
        //printf("Trixel: %4d %s\n", trixel, skyMesh()->indexToName( trixel ) );

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

    QPointF point( p->ra()->Hours(), p->dec()->Degrees() );
    QPointF wrapPoint( p->ra()->Hours() - 24.0, p->dec()->Degrees() );
    bool wrapRA = p->ra()->Hours() > 12.0;

    while ( iter != polyHash.constEnd() ) {

        PolyList* polyList = iter.key();
        iter++;

        //kDebug() << QString("checking %1 boundary\n").arg( polyList->name() );

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



