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
#include "skyobjects/stardata.h"
#include "skyobjects/deepstardata.h"
#include "starcomponent.h"

#include <kde_file.h>

StarBlockList::StarBlockList( Trixel tr, DeepStarComponent *parent ) {
    trixel = tr;
    nStars = 0;
    readOffset = 0;
    faintMag = -5.0;
    nBlocks = 0;
    this->parent = parent;
    staticStars = parent->hasStaticStars();
}

StarBlockList::~StarBlockList() {
    // NOTE: Rest of the StarBlocks are taken care of by StarBlockFactory
    if( staticStars && blocks[ 0 ] )
        delete blocks[0];
}

int StarBlockList::releaseBlock( StarBlock *block ) {

    if( block != blocks[ nBlocks - 1 ] )
        kDebug() << "ERROR: Trying to release a block which is not the last block! Trixel = " << trixel << endl;

    else if( blocks.size() > 0 ) {

        blocks.removeLast();
        nBlocks--;
        nStars -= block->getStarCount();

        readOffset -= parent->getStarReader()->guessRecordSize() * block->getStarCount();
	if( nBlocks <= 0 )
	  faintMag = -5.0;
	else
	  faintMag = blocks[nBlocks - 1]->faintMag;

        return 1;
    }

    return 0;
}

bool StarBlockList::fillToMag( float maglim ) {
    // TODO: Remove staticity of BinFileHelper
    BinFileHelper *dSReader;
    StarBlockFactory *SBFactory;
    starData stardata;
    deepStarData deepstardata;
    FILE *dataFile;

    dSReader = parent->getStarReader();
    dataFile = dSReader->getFileHandle();
    SBFactory = StarBlockFactory::Instance();

    if( staticStars )
        return false;

    if( faintMag >= maglim )
        return true;

    if( !dataFile ) {
        kDebug() << "dataFile not opened!";
        return false;
    }

    Trixel trixelId = trixel; //( ( trixel < 256 ) ? ( trixel + 256 ) : ( trixel - 256 ) ); // Trixel ID on datafile is assigned differently

    if( readOffset <= 0 )
        readOffset = dSReader->getOffset( trixelId );

    Q_ASSERT( nBlocks == blocks.size() );

    KDE_fseek( dataFile, readOffset, SEEK_SET );
    
    /*
    kDebug() << "Reading trixel" << trixel << ", id on disk =" << trixelId << ", currently nStars =" << nStars
             << ", record count =" << dSReader->getRecordCount( trixelId ) << ", first block = " << blocks[0]->getStarCount()
             << "to maglim =" << maglim << "with current faintMag =" << faintMag << endl;
    */

    while( maglim >= faintMag && nStars < dSReader->getRecordCount( trixelId ) ) {
        if( nBlocks == 0 || blocks[nBlocks - 1]->isFull() ) {
            StarBlock *newBlock;
            newBlock = SBFactory->getBlock();
            if( !newBlock ) {
                kWarning() << "ERROR: Could not get a new block from StarBlockFactory::getBlock() in trixel " 
                         << trixel << ", while trying to create block #" << nBlocks + 1 << endl;
                return false;
            }
            blocks.append( newBlock );
            blocks[nBlocks]->parent = this;
            if( nBlocks == 0 )
	        SBFactory->markFirst( blocks[0] );
	    else if( !SBFactory->markNext( blocks[nBlocks - 1], blocks[nBlocks] ) )
	        kWarning() << "ERROR: markNext() failed on block #" << nBlocks + 1 << "in trixel" << trixel;
            
            ++nBlocks;
        }
	// TODO: Make this more general
	if( dSReader->guessRecordSize() == 32 ) {
        fread( &stardata, sizeof( starData ), 1, dataFile );
        if( dSReader->getByteSwap() )
            DeepStarComponent::byteSwap( &stardata );
        readOffset += sizeof( starData );
        blocks[nBlocks - 1]->addStar(stardata);
	}
	else {
        fread( &deepstardata, sizeof( deepStarData ), 1, dataFile );
        if( dSReader->getByteSwap() )
            DeepStarComponent::byteSwap( &deepstardata );
        readOffset += sizeof( deepStarData );
        blocks[nBlocks - 1]->addStar(deepstardata);
	}

    /*
      if( faintMag > -5.0 && fabs(faintMag - blocks[nBlocks - 1]->getFaintMag()) > 0.2 ) {
      kDebug() << "Encountered a jump from mag" << faintMag << "to mag"
      << blocks[nBlocks - 1]->getFaintMag() << "in trixel" << trixel;
      }
    */
    faintMag = blocks[nBlocks - 1]->getFaintMag();
    nStars++;
    }

    return ( ( maglim < faintMag ) ? true : false );
}

void StarBlockList::setStaticBlock( StarBlock *block ) {
    if( !block )
        return;
    if ( nBlocks == 0 ) {
        blocks.append( block );
    }
    else 
        blocks[0] = block;

    blocks[0]->parent = this;
    faintMag = blocks[0]->faintMag;
    nBlocks = 1;
    staticStars = true;
}
