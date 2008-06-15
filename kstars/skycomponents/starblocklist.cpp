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
                                          readOffset(0), faintMag(-5.0) {
}

StarBlockList::~StarBlockList() {
    // NOTE: Rest of the StarBlocks are taken care of by StarBlockFactory
    if( blocks[0] )
        delete blocks[0];
}

int StarBlockList::releaseBlock( StarBlock *block ) {
    int nRemoved;
    nRemoved = blocks.removeAll( block );
    nBlocks -= nRemoved;
    faintMag = blocks[nBlocks - 1]->faintMag;
    return nRemoved;
}

bool StarBlockList::fillToMag( float maglim ) {
    BinFileHelper *dSReader = &StarComponent::deepStarReader;
    StarBlockFactory *SBFactory = &StarComponent::m_StarBlockFactory;
    FILE *dataFile = dSReader->getFileHandle();
    StarObject star;
    starData stardata;

    if( nBlocks == 0 )
        return false;

    if( faintMag >= maglim )
        return true;

    if( nBlocks == 1 ) {
        StarBlock *newBlock;
        nStars = blocks[0]->getStarCount();
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

    Trixel trixelId = ( ( trixel < 256 ) ? ( trixel + 256 ) : ( trixel - 256 ) ); // Trixel ID on datafile is assigned differently

    if( readOffset == 0 ) {
        readOffset = dSReader->getOffset( trixelId );
    }
    
    fseek( dataFile, readOffset, SEEK_SET );
    /*
    kDebug() << "Reading trixel " << trixel << ", id on disk = " << trixelId << ", record count = " 
             << dSReader->getRecordCount( trixelId ) << ", first block = " << blocks[0]->getStarCount();
    */
    while( maglim >= faintMag && nStars < dSReader->getRecordCount( trixelId ) + blocks[0]->getStarCount() ) {
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
        StarComponent::byteSwap( &stardata );
        readOffset += sizeof( starData );

        star.init( &stardata );
        blocks[nBlocks - 1]->addStar( &star );
        faintMag = blocks[nBlocks - 1]->faintMag;
        nStars++;
    }


    return ( ( maglim < faintMag ) ? true : false );
}

void StarBlockList::setStaticBlock( StarBlock *block ) {
    if ( block && nBlocks == 0 ) {
        blocks.append( block );
        faintMag = blocks[0]->faintMag;
        nBlocks = 1;
    }
}
