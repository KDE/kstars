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
#include <QMutexLocker>
#include <QDebug>

#include <KLocalizedString>

#include "Options.h"
#include "kstarsdata.h"
#include "skyobjects/skyobject.h"
#ifndef KSTARS_LITE
#include "skymap.h"
#endif

#include "skymesh.h"
#include "linelist.h"

#include "skypainter.h"


LineListIndex::LineListIndex( SkyComposite *parent, const QString& name ) :
    SkyComponent( parent ),
    m_name(name)
{
    m_skyMesh = SkyMesh::Instance();
    m_lineIndex = new LineListHash();
    m_polyIndex = new LineListHash();
}

LineListIndex::~LineListIndex()
{
    delete m_lineIndex;
    delete m_polyIndex;
}

// This is a callback for the indexLines() function below
const IndexHash& LineListIndex::getIndexHash(LineList* lineList ) {
    return skyMesh()->indexLine( lineList->points() );
}

void LineListIndex::removeLine( LineList* lineList)
{
    const IndexHash& indexHash = getIndexHash( lineList );
    IndexHash::const_iterator iter = indexHash.constBegin();
    while ( iter != indexHash.constEnd() )
    {
        Trixel trixel = iter.key();
        iter++;

         if (m_lineIndex->contains( trixel ))
                m_lineIndex->value( trixel )->removeOne( lineList );

    }

    m_listList.removeOne(lineList);
}

void LineListIndex::appendLine( LineList* lineList, int debug)
{
    if( debug < skyMesh()->debug() )
        debug = skyMesh()->debug();

    const IndexHash& indexHash = getIndexHash( lineList );
    IndexHash::const_iterator iter = indexHash.constBegin();
    while ( iter != indexHash.constEnd() ) {
        Trixel trixel = iter.key();
        iter++;

        if ( ! m_lineIndex->contains( trixel ) ) {
            m_lineIndex->insert(trixel, new LineListList() );
        }
        m_lineIndex->value( trixel )->append( lineList );
    }

    m_listList.append( lineList);
}

void LineListIndex::appendPoly(LineList* lineList, int debug)
{
    if ( debug < skyMesh()->debug() ) debug = skyMesh()->debug();

    const IndexHash& indexHash = skyMesh()->indexPoly( lineList->points() );
    IndexHash::const_iterator iter = indexHash.constBegin();
    while ( iter != indexHash.constEnd() ) {
        Trixel trixel = iter.key();
        iter++;

        if ( ! m_polyIndex->contains( trixel ) ) {
            m_polyIndex->insert( trixel, new LineListList() );
        }
        m_polyIndex->value( trixel )->append( lineList );
    }
}

void LineListIndex::appendBoth(LineList* lineList, int debug)
{
    QMutexLocker m1(&mutex);
    appendLine( lineList, debug );
    appendPoly( lineList, debug );
}

void LineListIndex::reindexLines()
{
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
void LineListIndex::preDraw( SkyPainter *skyp )
{
    skyp->setPen( QPen( QBrush( QColor( "white" ) ), 1, Qt::SolidLine ) );
}

void LineListIndex::draw( SkyPainter *skyp )
{
    if ( ! selected() )
        return;
    preDraw( skyp );
    drawLines( skyp );
}
#ifdef KSTARS_LITE
MeshIterator LineListIndex::visibleTrixels() {
    return MeshIterator( skyMesh(), drawBuffer() );
}
#endif

// This is a callback used int drawLinesInt() and drawLinesFloat()
SkipList* LineListIndex::skipList( LineList *lineList )
{
    Q_UNUSED(lineList)
    return 0;
}

void LineListIndex::drawLines( SkyPainter *skyp )
{
    DrawID   drawID   = skyMesh()->drawID();
    UpdateID updateID = KStarsData::Instance()->updateID();

    foreach ( LineListList* lineListList, m_lineIndex->values() ) {
        for (int i = 0; i < lineListList->size(); i++) {
            LineList* lineList = lineListList->at( i );

            if ( lineList->drawID == drawID )
                continue;
            lineList->drawID = drawID;

            if ( lineList->updateID != updateID )
                JITupdate( lineList );

            skyp->drawSkyPolyline(lineList, skipList(lineList), label() );
        }
    }
}

void LineListIndex::drawFilled( SkyPainter *skyp )
{
    DrawID drawID     = skyMesh()->drawID();
    UpdateID updateID = KStarsData::Instance()->updateID();

    MeshIterator region( skyMesh(), drawBuffer() );
    while ( region.hasNext() ) {

        LineListList* lineListList =  m_polyIndex->value( region.next() );
        if ( lineListList == 0 ) continue;

        for (int i = 0; i < lineListList->size(); i++) {
            LineList* lineList = lineListList->at( i );

            // draw each Linelist at most once
            if ( lineList->drawID == drawID ) continue;
            lineList->drawID = drawID;

            if ( lineList->updateID != updateID )
                JITupdate( lineList );

            skyp->drawSkyPolygon(lineList);
        }
    }
}

void LineListIndex::intro()
{
    emitProgressText( i18n( "Loading %1", m_name ));
    if ( skyMesh()->debug() >= 1 )
        qDebug() << QString("Loading %1 ...").arg( m_name );
}

void LineListIndex::summary()
{
    if ( skyMesh()->debug() < 2 )
        return;

    int total = skyMesh()->size();
    int polySize = m_polyIndex->size();
    int lineSize = m_lineIndex->size();

    if ( lineSize > 0 )
        printf("%4d out of %4d trixels in line index %3d%%\n",
               lineSize, total, 100 * lineSize / total );

    if ( polySize > 0 )
        printf("%4d out of %4d trixels in poly index %3d%%\n",
               polySize, total, 100 * polySize / total );

}
