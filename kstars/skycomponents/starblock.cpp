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


StarObject *StarBlock::plainStarTemplate = NULL;
int StarBlock::refCount = 0;

StarBlock::StarBlock() {
    init( NULL, NULL );
    allocStars( 100 );                // TODO: Make this number user-specifiable
}

StarBlock::StarBlock( int nstars ) {
    init( NULL, NULL );
    allocStars( nstars );
}

StarBlock::StarBlock( StarBlock *_prev, StarBlock *_next ) {
    init( _prev, _next );
    allocStars( 100 );                // TODO: Make this number user-specifiable
}

void StarBlock::init( StarBlock *_prev, StarBlock *_next ) {
    parent = NULL;
    prev = _prev;
    next = _next;
    drawID = 0;
    if( !plainStarTemplate )
        plainStarTemplate = new StarObject;
    refCount++;
    faintMag = -5.0;
    brightMag = -5.0;
    starsRead = 0;
}


StarBlock::~StarBlock() {
    StarObject *star;
    foreach( star, stars )
        free( star );
    refCount--;
    if(refCount == 0) {
        delete plainStarTemplate;
        plainStarTemplate == NULL;
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
