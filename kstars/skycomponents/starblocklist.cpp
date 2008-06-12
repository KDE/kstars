/***************************************************************************
                  starblocklist.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Mon 9 Jun 2008
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

#include "starblocklist.h"
#include "binfilehelper.h"
#include "starblockfactory.h"
#include "stardata.h"
#include "starcomponent.h"

StarBlockList::StarBlockList(Trixel tr) : trixel(tr), nStars(0), nBlocks(0),
                                          readOffset(0) {
    // TODO: Implement this if necessary
}

StarBlockList::~StarBlockList() {
    if( blocks[0] )
        delete blocks[0];
}

void StarBlockList::releaseBlock( StarBlock *block ) {
    nBlocks -= blocks.removeAll( block );
}

bool StarBlockList::fillToMag( float maglim ) {
    BinFileHelper *dSReader = &StarComponent::deepStarReader;
    StarBlockFactory *SBFactory = &StarComponent::m_StarBlockFactory;
    FILE *dataFile = dSReader->getFileHandle();
    StarObject star;
    starData stardata;

    if( nBlocks == 0 )
        return false;

    if( blocks[nBlocks - 1]->faintMag >= maglim )
        return true;

    if( nBlocks == 1 ) {
        StarBlock *newBlock;
        nStars += blocks[0]->getStarCount();
        newBlock =  SBFactory->getBlock();
        if(!newBlock) {
            kDebug() << "Could not get new block from StarBlockFactory::getBlock() in trixel " 
                     << trixel << " while trying to create a second block" << endl;
            return false;
        }
        blocks.append( newBlock );
        SBFactory->markFirst( blocks[1] );
        nBlocks = 2;
    }

    if( !dataFile )
        return false;

    if( readOffset == 0 ) {
        Trixel i = ( ( trixel < 256 ) ? ( trixel + 256 ) : ( trixel - 256 ) ); // Trixel ID on datafile is assigned differently
        readOffset = dSReader->getOffset( i );
    }
    
    fseek( dataFile, readOffset, SEEK_SET );
    
    while( maglim >= blocks[nBlocks - 1]->faintMag && nStars < dSReader->getRecordCount( trixel ) + blocks[0]->getStarCount() ) {
        if( blocks[nBlocks - 1]->isFull() ) {
            blocks.append( SBFactory->getBlock() );
            if( !blocks[nBlocks] ) {
                kDebug() << "ERROR: Could not get a new block from StarBlockFactory::getBlock() in trixel " 
                         << trixel << ", while trying to create block #" << nBlocks + 1 << endl;
                return false;
            }
            SBFactory->markNext( blocks[nBlocks - 1], blocks[nBlocks] );
            ++nBlocks;
        }
        fread( &stardata, sizeof( starData ), 1, dataFile );
        // TODO: Implement Byteswapping
        if( maglim < blocks[nBlocks - 1]->faintMag )
            break;
        readOffset += sizeof( starData );
        star.init( &stardata );
        blocks[nBlocks - 1]->addStar( &star );
        nStars++;
    }
    return true;
}

void StarBlockList::setStaticBlock( StarBlock *block ) {
    if ( block && nBlocks == 0 ) {
        blocks.append( block );
        nBlocks = 1;
    }
}
