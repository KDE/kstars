/***************************************************************************
                 linelistindex.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 2007-07-04
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

/****************************************************************************
 * The filled polygon code in the innermost loops below in drawFilled*() below
 * implements the Sutherland-Hodgman's Polygon clipping algorithm.  Please
 * don't mess with it unless you ensure that the Milky Way clipping continues
 * to work.  The line clipping uses a similar but slightly less complicated
 * algorithm.
 *
 * Since the clipping code is a bit messy, there are four versions of the
 * inner loop for Filled/Outline * Integer/Float.  This moved these two
 * decisions out of the inner loops to make them a bit faster and less
 * messy.
 *
 * -- James B. Bowlin
 *
 ****************************************************************************/

#include "linelistindex.h"

#include <QBrush>
#include <QPainter>

#include <kdebug.h>
#include <klocale.h>

#include "Options.h"
#include "kstarsdata.h"
#include "skyobjects/skyobject.h"
#include "skymap.h"

#include "skymesh.h"
#include "linelist.h"


LineListIndex::LineListIndex( SkyComponent *parent, const QString& name )
        : SkyComposite( parent ), m_name(name)
{
    m_skyMesh = SkyMesh::Instance();
    m_lineIndex = new LineListHash();
    m_polyIndex = new LineListHash();
    m_lineIndexCnt = m_polyIndexCnt = 0;
}

// This is a callback for the indexLines() function below
const IndexHash& LineListIndex::getIndexHash(LineList* lineList ) {
    return skyMesh()->indexLine( lineList->points() );
}


void LineListIndex::appendLine( LineList* lineList, int debug)
{

    if ( debug < skyMesh()->debug() ) debug = skyMesh()->debug();

    const IndexHash& indexHash = getIndexHash( lineList );
    IndexHash::const_iterator iter = indexHash.constBegin();
    while ( iter != indexHash.constEnd() ) {
        Trixel trixel = iter.key();
        iter++;

        if ( ! lineIndex()->contains( trixel ) ) {
            lineIndex()->insert(trixel, new LineListList() );
        }
        lineIndex()->value( trixel )->append( lineList );
    }

    m_listList.append( lineList);

    if ( debug > 9 )
        printf("LineList: %3d: %d\n", ++m_lineIndexCnt, indexHash.size() );
}

void LineListIndex::appendPoly(LineList* lineList, int debug)
{
    if ( debug < skyMesh()->debug() ) debug = skyMesh()->debug();

    const IndexHash& indexHash = skyMesh()->indexPoly( lineList->points() );
    IndexHash::const_iterator iter = indexHash.constBegin();
    while ( iter != indexHash.constEnd() ) {
        Trixel trixel = iter.key();
        iter++;

        if ( ! polyIndex()->contains( trixel ) ) {
            polyIndex()->insert( trixel, new LineListList() );
        }
        polyIndex()->value( trixel )->append( lineList );
    }

    if ( debug > 9 )
        printf("PolyList: %3d: %d\n", ++m_polyIndexCnt, indexHash.size() );
}

void LineListIndex::appendBoth(LineList* lineList, int debug)
{
    appendLine( lineList, debug );
    appendPoly( lineList, debug );
}

void LineListIndex::reindexLines()
{
    m_lineIndexCnt = 0;

    LineListHash* oldIndex = m_lineIndex;
    m_lineIndex = new LineListHash();

    DrawID drawID = skyMesh()->incDrawID();

    foreach (LineListList* listList, *oldIndex ) {
        for ( int i = 0; i < listList->size(); i++) {
            LineList* lineList = listList->at( i );
            if ( lineList->drawID == drawID ) continue;
            lineList->drawID = drawID;
            appendLine( lineList );
        }
        delete listList;
    }
    delete oldIndex;
}


void LineListIndex::JITupdate( LineList* lineList )
{
    KStarsData *data = KStarsData::Instance();
    lineList->updateID = data->updateID();
    SkyList* points = lineList->points();

    if ( lineList->updateNumID != data->updateNumID() ) {
        lineList->updateNumID = data->updateNumID();
        KSNumbers* num = data->updateNum();
        for (int i = 0; i < points->size(); i++ ) {
            points->at( i )->updateCoords( num );
        }
    }

    for (int i = 0; i < points->size(); i++ ) {
        points->at( i )->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
    }
}


// This is a callback used in draw() below
void LineListIndex::preDraw( QPainter &psky )
{
    psky.setPen( QPen( QBrush( QColor( "white" ) ), 1, Qt::SolidLine ) );
}

void LineListIndex::draw( QPainter &psky )
{
    if ( ! selected() )
        return;
    preDraw( psky );
    drawLines( psky );
}

// This is a callback used int drawLinesInt() and drawLinesFloat()
bool LineListIndex::skipAt( LineList *lineList, int i )
{
    Q_UNUSED(lineList)
    Q_UNUSED(i)
    return false;
}

void LineListIndex::updateLabelCandidates( const QPointF& /*o*/, LineList* /*lineList*/, int /*i*/ )
{}

void LineListIndex::drawAllLines( QPainter& psky )
{
    SkyMap *map = SkyMap::Instance();
    KStarsData *data = KStarsData::Instance();
    UpdateID updateID = data->updateID();

    QPolygonF polyMW;
    bool isVisible, isVisibleLast;
    SkyPoint  *pLast, *pThis;
    QPointF oThis, oLast, oMid;

    for (int i = 0; i < m_listList.size(); i++) {
        LineList* lineList = m_listList.at( i );

        if ( lineList->updateID != updateID )
            JITupdate( lineList );

        SkyList* points = lineList->points();
        pLast = points->first();
        oLast = map->toScreen( pLast, true, &isVisibleLast );

        for ( int j = 1 ; j < points->size() ; j++ ) {
            pThis = points->at( j );
            oThis = map->toScreen( pThis, true, &isVisible );

            if ( map->onScreen( oThis, oLast) && ! skipAt( lineList, j ) ) {
                if ( isVisible && isVisibleLast ) {
                    psky.drawLine( oLast, oThis );
                    updateLabelCandidates( oThis, lineList, j );
                } else if ( isVisibleLast ) {
                    oMid = map->clipLineI( pLast, pThis );
                    psky.drawLine( oLast, oMid );
                } else if ( isVisible ) {
                    oMid = map->clipLineI( pThis, pLast );
                    psky.drawLine( oMid, oThis );
                }
            }

            pLast = pThis;
            oLast = oThis;
            isVisibleLast = isVisible;
        }
    }
}


void LineListIndex::drawLines( QPainter& psky )
{
    SkyMap *map = SkyMap::Instance();
    DrawID   drawID   = skyMesh()->drawID();
    UpdateID updateID = KStarsData::Instance()->updateID();

    bool isVisible, isVisibleLast;

    MeshIterator region( skyMesh(), drawBuffer() );
    while ( region.hasNext() ) {

        LineListList* lineListList = lineIndex()->value( region.next() );
        if ( lineListList == 0 )
            continue;

        for (int i = 0; i < lineListList->size(); i++) {
            LineList* lineList = lineListList->at( i );

            if ( lineList->drawID == drawID )
                continue;
            lineList->drawID = drawID;

            if ( lineList->updateID != updateID )
                JITupdate( lineList );

            SkyList* points = lineList->points();
            SkyPoint* pLast = points->first();
            QPointF   oLast = map->toScreen( pLast, true, &isVisibleLast );

            QPointF oThis, oThis2;
            for ( int j = 1 ; j < points->size() ; j++ ) {
                SkyPoint* pThis = points->at( j );
                oThis2 = oThis = map->toScreen( pThis, true, &isVisible );

                if ( map->onScreen( oThis, oLast) && ! skipAt( lineList, j ) ) {

                    if ( isVisible && isVisibleLast && map->onscreenLine( oLast, oThis ) ) {
                        psky.drawLine( oLast, oThis );
                        updateLabelCandidates( oThis, lineList, j );
                    } else if ( isVisibleLast ) {
                        QPointF oMid = map->clipLine( pLast, pThis );
                        psky.drawLine( oLast, oMid );
                    } else if ( isVisible ) {
                        QPointF oMid = map->clipLine( pThis, pLast );
                        psky.drawLine( oMid, oThis );
                    }
                }

                pLast = pThis;
                oLast = oThis2;
                isVisibleLast = isVisible;
            }
        }
    }
}


void LineListIndex::drawFilled( QPainter& psky )
{
    SkyMap *map = SkyMap::Instance();
    DrawID drawID     = skyMesh()->drawID();
    UpdateID updateID = KStarsData::Instance()->updateID();

    bool isVisible, isVisibleLast;

    MeshIterator region( skyMesh(), drawBuffer() );
    while ( region.hasNext() ) {

        LineListList* lineListList =  polyIndex()->value( region.next() );
        if ( lineListList == 0 ) continue;

        for (int i = 0; i < lineListList->size(); i++) {
            LineList* lineList = lineListList->at( i );

            // draw each Linelist at most once
            if ( lineList->drawID == drawID ) continue;
            lineList->drawID = drawID;

            if ( lineList->updateID != updateID )
                JITupdate( lineList );

            SkyList* points = lineList->points();
            SkyPoint* pLast = points->last();
            QPointF   oLast = map->toScreen( pLast, true, &isVisibleLast );

            QPolygonF polygon;
            for ( int i = 0; i < points->size(); ++i ) {
                SkyPoint* pThis = points->at( i );
                QPointF oThis = map->toScreen( pThis, true, &isVisible );

                if ( isVisible && isVisibleLast ) {
                    polygon << oThis;
                } else if ( isVisibleLast ) {
                    QPointF oMid = map->clipLine( pLast, pThis );
                    polygon << oMid;
                } else if ( isVisible ) {
                    QPointF oMid = map->clipLine( pThis, pLast );
                    polygon << oMid;
                    polygon << oThis;
                }

                pLast = pThis;
                oLast = oThis;
                isVisibleLast = isVisible;
            }

            if ( polygon.size() )
                psky.drawPolygon( polygon );
        }
    }
    //psky.setRenderHint(QPainter::Antialiasing, antiAlias );
}

void LineListIndex::intro()
{
    emitProgressText( i18n( "Loading %1", name() ));

    if ( skyMesh()->debug() < 1 ) return;
    kDebug() << QString("Loading %1 ...").arg( name() );
}

void LineListIndex::summary()
{
    if ( skyMesh()->debug() < 2 ) return;

    int total = skyMesh()->size();
    int polySize = polyIndex()->size();
    int lineSize = lineIndex()->size();

    if ( lineSize > 0 )
        printf("%4d out of %4d trixels in line index %3d%%\n",
               lineSize, total, 100 * lineSize / total );

    if ( polySize > 0 )
        printf("%4d out of %4d trixels in poly index %3d%%\n",
               polySize, total, 100 * polySize / total );

}
