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

#include "milkyway.h"

#include <QList>
#include <QPointF>
#include <QPolygonF>
#include <QtConcurrent>

#include <KLocalizedString>

#include "kstarsdata.h"
#ifdef KSTARS_LITE
#include "skymaplite.h"
#else
#include "skymap.h"
#endif
#include "skyobjects/skypoint.h"
#include "dms.h"
#include "Options.h"
#include "ksfilereader.h"
#include "skycomponents/skiplist.h"

#include "skymesh.h"

#include "skypainter.h"


MilkyWay::MilkyWay( SkyComposite *parent ) :
    LineListIndex( parent, i18n("Milky Way") )
{
    intro();
    // Milky way
    //loadContours("milkyway.dat", i18n("Loading Milky Way"));
    // Magellanic clouds
    //loadContours("lmc.dat", i18n("Loading Large Magellanic Clouds"));
    //loadContours("smc.dat", i18n("Loading Small Magellanic Clouds"));
    //summary();

    QtConcurrent::run(this, &MilkyWay::loadContours, QString("milkyway.dat"), i18n("Loading Milky Way"));
    QtConcurrent::run(this, &MilkyWay::loadContours, QString("lmc.dat"), i18n("Loading Large Magellanic Clouds"));
    QtConcurrent::run(this, &MilkyWay::loadContours, QString("smc.dat"), i18n("Loading Small Magellanic Clouds"));
}

const IndexHash& MilkyWay::getIndexHash(LineList* lineList ) {
    // FIXME: EVIL!
    SkipList* skipList = (SkipList*) lineList;
    return skyMesh()->indexLine( skipList->points(), skipList->skipHash() );
}

SkipList* MilkyWay::skipList( LineList* lineList ) {
    // FIXME: EVIL!
    SkipList* skipList = (SkipList*) lineList;
    return skipList;
}

bool MilkyWay::selected()
{
#ifndef KSTARS_LITE
    return Options::showMilkyWay() &&
           ! ( Options::hideOnSlew() && Options::hideMilkyWay() && SkyMap::IsSlewing() );
#else
    return Options::showMilkyWay() &&
           ! ( Options::hideOnSlew() && Options::hideMilkyWay() && SkyMapLite::IsSlewing() );
#endif
}

void MilkyWay::draw( SkyPainter *skyp )
{
    if ( !selected() )
        return;

    QColor color = KStarsData::Instance()->colorScheme()->colorNamed( "MWColor" );
    skyp->setPen( QPen( color, 3, Qt::SolidLine ) );
    skyp->setBrush( QBrush( color ) );

    if( Options::fillMilkyWay() ) {
        drawFilled(skyp);
    } else {
        drawLines(skyp);
    }
}

void MilkyWay::loadContours(QString fname, QString greeting) {
    
    KSFileReader fileReader;
    if ( !fileReader.open( fname ) )
        return;
    fileReader.setProgress( greeting, 2136, 5 );

    SkipList *skipList = 0;
    int iSkip = 0;
    while ( fileReader.hasMoreLines() ) {
        QString line = fileReader.readLine();
        fileReader.showProgress();

        QChar firstChar = line.at( 0 );
        if ( firstChar == '#' )
            continue;

        bool okRA, okDec;
        double ra  = line.mid( 2,  8 ).toDouble(&okRA);
        double dec = line.mid( 11, 8 ).toDouble(&okDec);
        if( !okRA || !okDec) {
            qDebug() << QString("%1: conversion error on line: %2\n").arg(fname).arg(fileReader.lineNumber());
            continue;
        }

        if ( firstChar == 'M' )  {
            if( skipList )
                appendBoth( skipList );
            skipList = 0;
            iSkip    = 0;
        }

        if( !skipList )
            skipList = new SkipList();

        skipList->append( new SkyPoint(ra, dec) );
        if ( firstChar == 'S' )
            skipList->setSkip( iSkip );
        iSkip++;
    }  
    if ( skipList )
        appendBoth( skipList );
}
