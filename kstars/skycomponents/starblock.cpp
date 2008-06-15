/***************************************************************************
                   starblock.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 9 Jun 2008
    copyright            : (C) 2008 by Akarsh Simha
    email                : akarshsimha@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "starblock.h"
#include "starobject.h"
#include "starcomponent.h"
#include "stardata.h"

StarObject *StarBlock::plainStarTemplate = NULL;
int StarBlock::refCount = 0;

StarBlock::StarBlock() {
    init();
    allocStars( 100 );                // TODO: Make this number user-specifiable
}

StarBlock::StarBlock( int nstars ) {
    init();
    allocStars( nstars );
}

void StarBlock::init() {
    parent = NULL;
    prev = next = NULL;
    drawID = 0;
    if( !plainStarTemplate )
        plainStarTemplate = new StarObject;
    refCount++;
    reset();
}

void StarBlock::reset() {
    if( parent )
        parent->releaseBlock( this );
    faintMag = -5.0;
    brightMag = 15.0;
    nStars = 0;
}


StarBlock::~StarBlock() {
    StarObject *star;
    foreach( star, stars )
        free( star );
    refCount--;
    if(refCount == 0) {
        delete plainStarTemplate;
        plainStarTemplate = NULL;
    }
    if( parent )
        parent -> releaseBlock( this );
}


int StarBlock::allocStars( int nstars ) {
    StarObject *newStarObject;
    for( int i = 0; i < nstars; ++i ) {
        newStarObject = (StarObject *) malloc( sizeof( StarObject ) );
        if( !newStarObject )
            return i;
        memcpy( newStarObject, &plainStarTemplate, sizeof( StarObject ) );
        stars.append( newStarObject );
    }
    return nstars;
}

bool StarBlock::addStar( StarObject *star ) {

    if(isFull())
        return false;

    memcpy( stars.at( nStars ), star, sizeof( StarObject ) );
    
    if( star->mag() > faintMag )
        faintMag = star->mag();
    if( star->mag() < brightMag )
        brightMag = star->mag();
    
    ++nStars;
    return true;
}
