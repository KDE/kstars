/***************************************************************************
                          skiplistindex.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 2007-07-08
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

#include "skiplistindex.h"

#include <QList>
#include <QPointF>

#include "skyobjects/skypoint.h"
#include "skymesh.h"
#include "ksfilereader.h"

#include "skiplist.h"


SkipListIndex::SkipListIndex( SkyComponent *parent, const QString& name )
        : LineListIndex(parent, name )
{}

const IndexHash& SkipListIndex::getIndexHash(LineList* lineList ) {
    SkipList* skipList = (SkipList*) lineList;
    return skyMesh()->indexLine( skipList->points(), skipList->skipHash() );
}

bool SkipListIndex::skipAt( LineList* lineList, int i ) {
    SkipList* skipList = (SkipList*) lineList;
    return skipList->skip( i );
}

void SkipListIndex::loadSkipLists(QString fname, QString greeting) {
    
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
            kDebug() << QString("%1: conversion error on line: %2\n").arg(fname).arg(fileReader.lineNumber());
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
