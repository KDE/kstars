/***************************************************************************
                 constellationlines.h  -  K Desktop Planetarium
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

#include "constellationlines.h"
#include "linelist.h"
#include "constellationsart.h"
#include <QPen>

#include <QDebug>
#include <KLocalizedString>

#include "Options.h"
#include "kstarsdata.h"
#include "skyobjects/skyobject.h"
#include "skyobjects/starobject.h"
#include "skycomponents/starcomponent.h"
#include "skycomponents/culturelist.h"
#include "skymap.h"

#include "skymesh.h"
#include "ksfilereader.h"

#include "skypainter.h"


ConstellationLines::ConstellationLines( SkyComposite *parent, CultureList* cultures ) :
    LineListIndex( parent, xi18n("Constellation Lines") ),
    m_reindexNum(J2000)
{
    //Create the ConstellationLinesComponents.  Each is a series of points
    //connected by line segments.  A single constellation can be composed of
    //any number of these series, and we don't need to know which series
    //belongs to which constellation.

    //The constellation lines data file (clines.dat) contains lists
    //of abbreviated genetive star names in the same format as they
    //appear in the star data files (hipNNN.dat).
    //
    //Each constellation consists of a QList of SkyPoints,
    //corresponding to the stars at each "node" of the constellation.
    //These are pointers to the starobjects themselves, so the nodes
    //will automatically be fixed to the stars even as the star
    //positions change due to proper motions.  In addition, each node
    //has a corresponding flag that determines whether a line should
    //connect this node and the previous one.

    intro();

    bool culture = false;
    LineList *lineList(0);
    double maxPM(0.0);

    KSFileReader fileReader;
    if ( ! fileReader.open( "clines.dat" ) )
        return;
    while ( fileReader.hasMoreLines() ) {
        QString line = fileReader.readLine();
        if( line.isEmpty() )
            continue;
        QChar mode = line.at( 0 );
        //ignore lines beginning with "#":
        if( mode == '#' )
            continue;
        //If the first character is "M", we are starting a new series.
        //In this case, add the existing clc component to the composite,
        //then prepare a new one.

        if ( mode == 'C') {
            QString cultureName = line.mid( 2 ).trimmed();
            culture = cultureName == cultures->current();
            continue;
        }

        if ( culture ) {
            //Mode == 'M' starts a new series of line segments, joined end to end
            if ( mode == 'M' ) {
                if( lineList )
                    appendLine( lineList );
                lineList = new LineList();
            }

            int HDnum = line.mid( 2 ).trimmed().toInt();
            StarObject *star = static_cast<StarObject*>( StarComponent::Instance()->findByHDIndex( HDnum ) );
            if ( star && lineList ) {
                lineList->append( star );
                double pm = star->pmMagnitude();
                if ( maxPM < pm )
                    maxPM = pm;
            }
            else if ( ! star )
                qWarning() << xi18n( "Star HD%1 not found." , HDnum);
        }
    }
    ConstellationsArt art(57);
    //Add the last clc component
    if( lineList )
        appendLine( lineList );

    m_reindexInterval = StarObject::reindexInterval( maxPM );
    //printf("CLines:           maxPM = %6.1f milliarcsec/year\n", maxPM );
    //printf("CLines: Update Interval = %6.1f years\n", m_reindexInterval * 100.0 );

    summary();
}

bool ConstellationLines::selected()
{
    return Options::showCLines() &&
           ! ( Options::hideOnSlew() && Options::hideCLines() && SkyMap::IsSlewing() );
}

void ConstellationLines::preDraw( SkyPainter* skyp )
{
    KStarsData *data = KStarsData::Instance();
    QColor color = data->colorScheme()->colorNamed( "CLineColor" );
    skyp->setPen( QPen( QBrush( color ), 1, Qt::SolidLine ) );
}

const IndexHash& ConstellationLines::getIndexHash(LineList* lineList ) {
    return skyMesh()->indexStarLine( lineList->points() );
}

// JIT updating makes this simple.  Star updates are called from within both
// StarComponent and ConstellationLines.  If the update is redundant then
// StarObject::JITupdate() simply returns without doing any work.
void ConstellationLines::JITupdate( LineList* lineList )
{
    KStarsData *data = KStarsData::Instance();
    lineList->updateID = data->updateID();

    SkyList* points = lineList->points();
    for (int i = 0; i < points->size(); i++ ) {
        StarObject* star = (StarObject*) points->at( i );
        star->JITupdate();
    }
}

void ConstellationLines::reindex( KSNumbers *num )
{
    if ( ! num ) return;

    if ( fabs( num->julianCenturies() -
               m_reindexNum.julianCenturies() ) < m_reindexInterval ) return;

    //printf("Re-indexing CLines to year %4.1f...\n", 2000.0 + num->julianCenturies() * 100.0);

    m_reindexNum = KSNumbers( *num );
    skyMesh()->setKSNumbers( num );
    LineListIndex::reindexLines();

    //printf("Done.\n");
}

