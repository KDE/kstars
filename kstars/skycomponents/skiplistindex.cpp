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



