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


#include <QBrush>
#include <QPainter>

#include <kdebug.h>
#include <klocale.h>

#include "kstars.h"
#include "Options.h"
#include "kstarsdata.h"
#include "ksutils.h"
#include "skyobject.h"
#include "skymap.h"

#include "skymesh.h"
#include "linelist.h"
#include "linelistindex.h"


LineListIndex::LineListIndex( SkyComponent *parent, const QString& name )
  : SkyComposite( parent ), m_name(name) 
{
    m_skyMesh = SkyMesh::Instance();
    m_lineIndex = new LineListHash();
    m_polyIndex = new LineListHash();
    m_lineIndexCnt = m_polyIndexCnt = 0;
}

// This is a callback for the indexLines() function below
const IndexHash& LineListIndex::getIndexHash(LineList* lineList )
{
     return skyMesh()->indexLine( lineList->points() );
}


void LineListIndex::appendLine( LineList* lineList, int debug)
{

    if ( debug < skyMesh()->debug() ) debug = skyMesh()->debug();

    const IndexHash& indexHash = getIndexHash( lineList );

    foreach ( Trixel trixel, indexHash.keys() ) {
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

    foreach (Trixel trixel, indexHash.keys()) {     // foreach okay because we
                                                 // we needed a copy anyway
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

    foreach (LineListList* listList, oldIndex->values() ) {
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


void LineListIndex::JITupdate( KStarsData *data, LineList* lineList )
{
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
void LineListIndex::preDraw( KStars *kstars, QPainter &psky )
{
    psky.setPen( QPen( QBrush( QColor( "white" ) ), 1, Qt::SolidLine ) );
}

void LineListIndex::draw( KStars *kstars, QPainter &psky, double scale )
{
    if ( ! selected() ) return;

    preDraw(kstars, psky);

    if ( Options::useAntialias() )
        drawLinesFloat( kstars, psky, scale );
    else
        drawLinesInt( kstars, psky, scale );
}

// This is a callback used int drawLinesInt() and drawLinesFloat()
bool LineListIndex::skipAt( LineList* lineList, int i ) 
{                                      // left this in .cpp because
    return false;                      // it generates compiler warnings.
}                                      // -jbb


// Yet another 2 callbacks.  These are used in LabeledListIndex
void LineListIndex::updateLabelCandidates( const QPointF& o, LineList* lineList, int i )
{}

void LineListIndex::updateLabelCandidates( const QPoint& o, LineList* lineList, int i )
{}

//-----------------------------------------------------------------------------
// The four^H^H six drawing routines below are very, very similar.  I think any
// further compactification of the code will move more decisions into the inner
// loops having an negative impact on performance.
//-----------------------------------------------------------------------------

void LineListIndex::drawAllLinesInt( KStars *kstars, QPainter& psky, double scale)
{
	SkyMap *map = kstars->map();
    UpdateID updateID = kstars->data()->updateID();
	bool isVisible, isVisibleLast;
    SkyPoint *pLast, *pThis;
    QPoint oThis, oLast, oMid;


    for (int i = 0; i < m_listList.size(); i++) {
        LineList* lineList = m_listList.at( i );

        if ( lineList->updateID != updateID ) 
            JITupdate( kstars->data(), lineList );

        SkyList* points = lineList->points();
        pLast = points->first();
        oLast = map->toScreenI( pLast, scale, false, &isVisibleLast );

        for ( int i = 1 ; i < points->size() ; i++ ) {
            pThis = points->at( i );
            oThis = map->toScreenI( pThis, scale, false, &isVisible );

            if ( ! skipAt( lineList, i ) ) {

                if ( isVisible && isVisibleLast ) {
                    psky.drawLine( oLast.x(), oLast.y(), oThis.x(), oThis.y() );
					updateLabelCandidates( oThis, lineList, i );
                }
                else if ( isVisibleLast ) {
                    oMid = map->clipLineI( pLast, pThis, scale );
                    psky.drawLine( oLast.x(), oLast.y(), oMid.x(), oMid.y() );
                }
                else if ( isVisible ) {
                    oMid = map->clipLineI( pThis, pLast, scale );
                    psky.drawLine( oMid.x(), oMid.y(), oThis.x(), oThis.y() );
                }
            }

            pLast = pThis;
            oLast = oThis;
            isVisibleLast = isVisible;
        }
    }
}


void LineListIndex::drawAllLinesFloat( KStars *kstars, QPainter& psky, double scale )
{
	SkyMap *map = kstars->map();
    UpdateID updateID = kstars->data()->updateID();
	QPolygonF polyMW;
	bool isVisible, isVisibleLast;
    SkyPoint  *pLast, *pThis;
    QPointF oThis, oLast, oMid;


    for (int i = 0; i < m_listList.size(); i++) {
        LineList* lineList = m_listList.at( i );
    
        if ( lineList->updateID != updateID ) 
            JITupdate( kstars->data(), lineList );

        SkyList* points = lineList->points();
        pLast = points->first();
        oLast = map->toScreenI( pLast, scale, false, &isVisibleLast );

        for ( int i = 1 ; i < points->size() ; i++ ) {
            pThis = points->at( i );
            oThis = map->toScreenI( pThis, scale, false, &isVisible );

            if ( ! skipAt( lineList, i ) ) {

                if ( isVisible && isVisibleLast ) {
                    psky.drawLine( oLast, oThis );
					updateLabelCandidates( oThis, lineList, i );
                }
                else if ( isVisibleLast ) {
                    oMid = map->clipLineI( pLast, pThis, scale );
                    psky.drawLine( oLast, oMid );
                }
                else if ( isVisible ) {
                    oMid = map->clipLineI( pThis, pLast, scale );
                    psky.drawLine( oMid, oThis );
                }
            }

            pLast = pThis;
            oLast = oThis;
            isVisibleLast = isVisible;
        }
    }
}


void LineListIndex::drawLinesInt( KStars *kstars, QPainter& psky, double scale)
{
	SkyMap *map = kstars->map();
    DrawID drawID = skyMesh()->drawID();
    UpdateID updateID = kstars->data()->updateID();
	bool isVisible, isVisibleLast;
    SkyPoint *pLast, *pThis;
    QPoint oThis, oLast, oMid;

    MeshIterator region( skyMesh(), drawBuffer() );
    while ( region.hasNext() ) {

        LineListList *lineListList = lineIndex()->value( region.next() );
        if ( lineListList == 0 ) continue;

        for (int i = 0; i < lineListList->size(); i++) {
            LineList* lineList = lineListList->at( i );

            //draw each LineList at most once
            if ( lineList->drawID == drawID ) continue;
            lineList->drawID = drawID;

            if ( lineList->updateID != updateID )
                JITupdate( kstars->data(), lineList );

            SkyList* points = lineList->points();
            pLast = points->first();
            oLast = map->toScreenI( pLast, scale, false, &isVisibleLast );

            for ( int i = 1 ; i < points->size() ; i++ ) {
                pThis = points->at( i );
                oThis = map->toScreenI( pThis, scale, false, &isVisible );

                if ( ! skipAt( lineList, i ) ) {

                    if ( isVisible && isVisibleLast ) {
                        psky.drawLine( oLast.x(), oLast.y(), oThis.x(), oThis.y() );
						updateLabelCandidates( oThis, lineList, i );
                    }
                    else if ( isVisibleLast ) {
                        oMid = map->clipLineI( pLast, pThis, scale );
                        psky.drawLine( oLast.x(), oLast.y(), oMid.x(), oMid.y() );
                    }
                    else if ( isVisible ) {
                        oMid = map->clipLineI( pThis, pLast, scale );
                        psky.drawLine( oMid.x(), oMid.y(), oThis.x(), oThis.y() );
                    }
                }

                pLast = pThis;
                oLast = oThis;
                isVisibleLast = isVisible;
            }
        }
    }
}


void LineListIndex::drawLinesFloat( KStars *kstars, QPainter& psky, double scale )
{
	SkyMap *map = kstars->map();
    DrawID drawID = skyMesh()->drawID();
    UpdateID updateID = kstars->data()->updateID();
	QPolygonF polyMW;
	bool isVisible, isVisibleLast;
    SkyPoint  *pLast, *pThis;
    QPointF oThis, oLast, oMid;

    MeshIterator region( skyMesh(), drawBuffer() );
    while ( region.hasNext() ) {

        LineListList* lineListList = lineIndex()->value( region.next() );
        if ( lineListList == 0 ) continue;

        for (int i = 0; i < lineListList->size(); i++) {
            LineList* lineList = lineListList->at( i );

            if ( lineList->drawID == drawID ) continue;
            lineList->drawID = drawID;

            if ( lineList->updateID != updateID ) 
                JITupdate( kstars->data(), lineList );

            SkyList* points = lineList->points();
            pLast = points->first();
            oLast = map->toScreenI( pLast, scale, false, &isVisibleLast );

            for ( int i = 1 ; i < points->size() ; i++ ) {
                pThis = points->at( i );
                oThis = map->toScreenI( pThis, scale, false, &isVisible );

                if ( ! skipAt( lineList, i ) ) {

                    if ( isVisible && isVisibleLast ) {
                        psky.drawLine( oLast, oThis );
						updateLabelCandidates( oThis, lineList, i );
                    }
                    else if ( isVisibleLast ) {
                        oMid = map->clipLineI( pLast, pThis, scale );
                        psky.drawLine( oLast, oMid );
                    }
                    else if ( isVisible ) {
                        oMid = map->clipLineI( pThis, pLast, scale );
                        psky.drawLine( oMid, oThis );
                    }
                }

                pLast = pThis;
                oLast = oThis;
                isVisibleLast = isVisible;
            }
        }
    }
}

void LineListIndex::drawFilledInt( KStars *kstars, QPainter& psky, double scale)
{
	SkyMap *map = kstars->map();
    DrawID drawID = skyMesh()->drawID();
    UpdateID updateID = kstars->data()->updateID();
	bool isVisible, isVisibleLast;
    SkyPoint *pLast, *pThis;
    QPolygon polygon;
    QPoint oThis, oLast, oMid;

    MeshIterator region( skyMesh(), drawBuffer() );
    while ( region.hasNext() ) {

        LineListList* lineListList = polyIndex()->value( region.next() );
        if ( lineListList == 0 ) continue;

        for (int i = 0; i < lineListList->size(); i++) {
            LineList* lineList = lineListList->at( i );

            //draw each LineList at most once
            if ( lineList->drawID == drawID ) continue;
            lineList->drawID = drawID;

            if ( lineList->updateID != updateID ) 
                JITupdate( kstars->data(), lineList );

            SkyList* points = lineList->points();
            pLast = points->last();
            oLast = map->toScreenI( pLast, scale, false, &isVisibleLast );

            for ( int i = 0; i < points->size(); ++i ) {
                pThis = points->at( i );
                oThis = map->toScreenI( pThis, scale, false, &isVisible );

                if ( isVisible && isVisibleLast ) {
                    polygon << oThis;
                }
                else if ( isVisibleLast ) {
                    oMid = map->clipLineI( pLast, pThis, scale );
                    polygon << oMid;
                }
                else if ( isVisible ) {
                    oMid = map->clipLineI( pThis, pLast, scale );
                    polygon << oMid;
                    polygon << oThis;
                }

                pLast = pThis;
                oLast = oThis;
                isVisibleLast = isVisible;
            }

            if ( polygon.size() ) psky.drawPolygon( polygon );
            polygon.clear();
        }
    }
}


void LineListIndex::drawFilledFloat( KStars *kstars, QPainter& psky, double scale )
{
	SkyMap *map = kstars->map();
    DrawID drawID = skyMesh()->drawID();
    UpdateID updateID = kstars->data()->updateID();

	QPolygonF polygon;
	bool isVisible, isVisibleLast;
    SkyPoint  *pLast, *pThis;
    QPointF oThis, oLast, oMid;

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
                JITupdate( kstars->data(), lineList );

            SkyList* points = lineList->points();
            pLast = points->last();
            oLast = map->toScreen( pLast, scale, false, &isVisibleLast );

            for ( int i = 0; i < points->size(); ++i ) {
                pThis = points->at( i );
                oThis = map->toScreen( pThis, scale, false, &isVisible );

                if ( isVisible && isVisibleLast ) {
                    polygon << oThis;
                }
                else if ( isVisibleLast ) {
                    oMid = map->clipLine( pLast, pThis, scale );
                    polygon << oMid;
                }
                else if ( isVisible ) {
                    oMid = map->clipLine( pThis, pLast, scale );
                    polygon << oMid;
                    polygon << oThis;
                }

                pLast = pThis;
                oLast = oThis;
                isVisibleLast = isVisible;
            }

            if ( polygon.size() ) psky.drawPolygon( polygon );
            polygon.clear();
        }
    }
}

void LineListIndex::intro()
{
    emitProgressText( i18n( "Loading %1", name() ));

    if ( skyMesh()->debug() < 1 ) return;
    kDebug() << QString("Loading %1 ...").arg( name() ) << endl;
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
