/***************************************************************************
                        deepstarcomponent.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Fri 1st Aug 2008
    copyright            : (C) 2008 Akarsh Simha, Thomas Kabelmann
    email                : akarshsimha@gmail.com, thomas.kabelmann@gmx.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifdef _WIN32
#include <windows.h>
#endif
#include "deepstarcomponent.h"

#include <QPixmap>
#include <QRectF>
#include <QFontMetricsF>

//NOTE Added this for QT_FSEEK, should we be including another file?
#include <qplatformdefs.h>

#include "Options.h"
#include "kstarsdata.h"
#include "skymap.h"
#include "skyobjects/starobject.h"
#include "skymesh.h"
#include "binfilehelper.h"
#include "starblockfactory.h"
#include "starcomponent.h"
#include "projections/projector.h"

#include "skypainter.h"

#include "byteorder.h"

DeepStarComponent::DeepStarComponent( SkyComposite *parent, QString fileName, float trigMag, bool staticstars ) :
    ListComponent(parent),
    m_reindexNum( J2000 ),
    triggerMag( trigMag ),
    m_FaintMagnitude(-5.0), 
    staticStars( staticstars ),
    dataFileName( fileName )
{
    fileOpened = false;
    openDataFile();
    if( staticStars )
        loadStaticStars();
    qDebug() << "Loaded DSO catalog file: " << dataFileName;
}

bool DeepStarComponent::loadStaticStars() {
    FILE *dataFile;

    if( !staticStars )
        return true;
    if( !fileOpened )
        return false;

    dataFile = starReader.getFileHandle();
    rewind( dataFile );

    if( !starReader.readHeader() ) {
        qDebug() << "Error reading header of catalog file " << dataFileName << ": " << starReader.getErrorNumber() << ": " << starReader.getError() << endl;
        return false;
    }

    if( starReader.guessRecordSize() != 16 && starReader.guessRecordSize() != 32 ) {
        qDebug() << "Cannot understand catalog file " << dataFileName << endl;
        return false;
    }

    //KDE_fseek(dataFile, starReader.getDataOffset(), SEEK_SET);
    QT_FSEEK(dataFile, starReader.getDataOffset(), SEEK_SET);

    qint16 faintmag;
    quint8 htm_level;
    quint16 t_MSpT;

    fread( &faintmag, 2, 1, dataFile );
    if( starReader.getByteSwap() )
        faintmag = bswap_16( faintmag );
    fread( &htm_level, 1, 1, dataFile );
    fread( &t_MSpT, 2, 1, dataFile ); // Unused
    if( starReader.getByteSwap() )
        faintmag = bswap_16( faintmag );


    // TODO: Read the multiplying factor from the dataFile
    m_FaintMagnitude = faintmag / 100.0;

    if( htm_level != m_skyMesh->level() )
        qDebug() << "WARNING: HTM Level in shallow star data file and HTM Level in m_skyMesh do not match. EXPECT TROUBLE" << endl;

    for(Trixel i = 0; i < (unsigned int)m_skyMesh->size(); ++i) {

        Trixel trixel = i;
        StarBlock *SB = new StarBlock( starReader.getRecordCount( i ) );
        if( !SB )
            qDebug() << "ERROR: Could not allocate new StarBlock to hold shallow unnamed stars for trixel " << trixel << endl;
        m_starBlockList.at( trixel )->setStaticBlock( SB );
        
        for(unsigned long j = 0; j < (unsigned long) starReader.getRecordCount(i); ++j) {
            bool fread_success = false;
            if( starReader.guessRecordSize() == 32 )
                fread_success = fread( &stardata, sizeof( starData ), 1, dataFile );
            else if( starReader.guessRecordSize() == 16 )
                fread_success = fread( &deepstardata, sizeof( deepStarData ), 1, dataFile );

            if( !fread_success ) {
                qDebug() << "ERROR: Could not read starData structure for star #" << j << " under trixel #" << trixel << endl;
            }

            /* Swap Bytes when required */            
            if( starReader.getByteSwap() ) {
                if( starReader.guessRecordSize() == 32 )
                    byteSwap( &stardata );
                else
                    byteSwap( &deepstardata );
            }

            /* Initialize star with data just read. */
            StarObject* star;
            if( starReader.guessRecordSize() == 32 )
                star = SB->addStar( stardata );
            else
                star = SB->addStar( deepstardata );
            
            if( star ) {
                KStarsData* data = KStarsData::Instance();
                star->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
                if( star->getHDIndex() != 0 )
                    m_CatalogNumber.insert( star->getHDIndex(), star );
            } else {
                qDebug() << "CODE ERROR: More unnamed static stars in trixel " << trixel << " than we allocated space for!" << endl;
            }
        }
    }

    return true;
}

DeepStarComponent::~DeepStarComponent() {
  if( fileOpened )
    starReader.closeFile();
  fileOpened = false;
}

bool DeepStarComponent::selected() {
    return Options::showStars() && fileOpened;
}

bool openIndexFile( ) {
    // TODO: Work out the details
    /*
    if( hdidxReader.openFile( "Henry-Draper.idx" ) )
        qDebug() << "Could not open HD Index file. Search by HD numbers for deep stars will not work." << endl;
    */
    return 0;
}

//This function is empty for a reason; we override the normal 
//update function in favor of JiT updates for stars.
void DeepStarComponent::update( KSNumbers * )
{}

// TODO: Optimize draw, if it is worth it.
void DeepStarComponent::draw( SkyPainter *skyp ) {
    if ( !fileOpened ) return;

    SkyMap *map = SkyMap::Instance();
    KStarsData* data = KStarsData::Instance();
    UpdateID updateID = data->updateID();

    //FIXME_FOV -- maybe not clamp like that...
    float radius = map->projector()->fov();
    if ( radius > 90.0 ) radius = 90.0;

    if ( m_skyMesh != SkyMesh::Instance() && m_skyMesh->inDraw() ) {
        printf("Warning: aborting concurrent DeepStarComponent::draw()");
    }
    bool checkSlewing = ( map->isSlewing() && Options::hideOnSlew() );

    //shortcuts to inform whether to draw different objects
    bool hideFaintStars( checkSlewing && Options::hideStars() );
    double hideStarsMag = Options::magLimitHideStar();

    //adjust maglimit for ZoomLevel
//    double lgmin = log10(MINZOOM);
//    double lgmax = log10(MAXZOOM);
//    double lgz = log10(Options::zoomFactor());
    // TODO: Enable hiding of faint stars

    float maglim = StarComponent::zoomMagnitudeLimit();

    if( maglim < triggerMag )
        return;

    m_zoomMagLimit = maglim;

    m_skyMesh->inDraw( true );

    SkyPoint* focus = map->focus();
    m_skyMesh->aperture( focus, radius + 1.0, DRAW_BUF ); // divide by 2 for testing

    MeshIterator region(m_skyMesh, DRAW_BUF);

    magLim = maglim;

    // If we are to hide the fainter stars (eg: while slewing), we set the magnitude limit to hideStarsMag.
    if( hideFaintStars && maglim > hideStarsMag )
        maglim = hideStarsMag;

    StarBlockFactory *m_StarBlockFactory = StarBlockFactory::Instance();
    //    m_StarBlockFactory->drawID = m_skyMesh->drawID();
    //    qDebug() << "Mesh size = " << m_skyMesh->size() << "; drawID = " << m_skyMesh->drawID();
    QTime t;
    int nTrixels = 0;

    t_dynamicLoad = 0;
    t_updateCache = 0;
    t_drawUnnamed = 0;

    visibleStarCount = 0;

    t.start();

    // Mark used blocks in the LRU Cache. Not required for static stars
    if( !staticStars ) {
        while( region.hasNext() ) {
            Trixel currentRegion = region.next();
            for( int i = 0; i < m_starBlockList.at( currentRegion )->getBlockCount(); ++i ) {
                StarBlock *prevBlock = ( ( i >= 1 ) ? m_starBlockList.at( currentRegion )->block( i - 1 ) : NULL );
                StarBlock *block = m_starBlockList.at( currentRegion )->block( i );
                
                if( i == 0  &&  !m_StarBlockFactory->markFirst( block ) )
                    qDebug() << "markFirst failed in trixel" << currentRegion;
                if( i > 0   &&  !m_StarBlockFactory->markNext( prevBlock, block ) )
                    qDebug() << "markNext failed in trixel" << currentRegion << "while marking block" << i;
                if( i < m_starBlockList.at( currentRegion )->getBlockCount() 
                    && m_starBlockList.at( currentRegion )->block( i )->getFaintMag() < maglim )
                    break;
                    
            }
        }
        t_updateCache = t.restart();
        region.reset();
    }

    while ( region.hasNext() ) {
        ++nTrixels;
        Trixel currentRegion = region.next();

        // NOTE: We are guessing that the last 1.5/16 magnitudes in the catalog are just additions and the star catalog
        //       is actually supposed to reach out continuously enough only to mag m_FaintMagnitude * ( 1 - 1.5/16 )
        // TODO: Is there a better way? We may have to change the magnitude tolerance if the catalog changes
        // Static stars need not execute fillToMag

	if( !staticStars && !m_starBlockList.at( currentRegion )->fillToMag( maglim ) && maglim <= m_FaintMagnitude * ( 1 - 1.5/16 ) ) {
            qDebug() << "SBL::fillToMag( " << maglim << " ) failed for trixel " 
                     << currentRegion << " !"<< endl;
	}

        t_dynamicLoad += t.restart();

        //        qDebug() << "Drawing SBL for trixel " << currentRegion << ", SBL has " 
        //                 <<  m_starBlockList[ currentRegion ]->getBlockCount() << " blocks" << endl;

        for( int i = 0; i < m_starBlockList.at( currentRegion )->getBlockCount(); ++i ) {
            StarBlock *block = m_starBlockList.at( currentRegion )->block( i );
            //            qDebug() << "---> Drawing stars from block " << i << " of trixel " << 
            //                currentRegion << ". SB has " << block->getStarCount() << " stars" << endl;
            for( int j = 0; j < block->getStarCount(); j++ ) {

                StarObject *curStar = block->star( j );

                //                qDebug() << "We claim that he's from trixel " << currentRegion 
                //<< ", and indexStar says he's from " << m_skyMesh->indexStar( curStar );

                if ( curStar->updateID != updateID )
                    curStar->JITupdate();

                float mag = curStar->mag();

                if ( mag > maglim )
                    break;

                if( skyp->drawPointSource(curStar, mag, curStar->spchar() ) )
                    visibleStarCount++;
            }
        }

        // DEBUG: Uncomment to identify problems with Star Block Factory / preservation of Magnitude Order in the LRU Cache
        //        verifySBLIntegrity();        
        t_drawUnnamed += t.restart();

    }
    m_skyMesh->inDraw( false );

}

bool DeepStarComponent::openDataFile() {

    if( starReader.getFileHandle() )
        return true;

    starReader.openFile( dataFileName );
    fileOpened = false;
    if( !starReader.getFileHandle() )
        qDebug() << "WARNING: Failed to open deep star catalog " << dataFileName << ". Disabling it." << endl;
    else if( !starReader.readHeader() )
        qDebug() << "WARNING: Header read error for deep star catalog " << dataFileName << "!! Disabling it!" << endl;
    else {
        qint16 faintmag;
        quint8 htm_level;
        fread( &faintmag, 2, 1, starReader.getFileHandle() );
        if( starReader.getByteSwap() )
            faintmag = bswap_16( faintmag );
        if( starReader.guessRecordSize() == 16 )
            m_FaintMagnitude = faintmag / 1000.0;
        else
            m_FaintMagnitude = faintmag / 100.0;
        fread( &htm_level, 1, 1, starReader.getFileHandle() );
        qDebug() << "Processing " << dataFileName << ", HTMesh Level" << htm_level;
        m_skyMesh = SkyMesh::Instance( htm_level );
        if( !m_skyMesh ) {
            if( !( m_skyMesh = SkyMesh::Create( htm_level ) ) ) {
                qDebug() << "Could not create HTMesh of level " << htm_level << " for catalog " << dataFileName << ". Skipping it.";
                return false;
            }
        }
        meshLevel = htm_level;
        fread( &MSpT, 2, 1, starReader.getFileHandle() );
        if( starReader.getByteSwap() )
            MSpT = bswap_16( MSpT );
        fileOpened = true;
        qDebug() << "  Sky Mesh Size: " << m_skyMesh->size();
        for (long int i = 0; i < m_skyMesh->size(); i++) {
            StarBlockList *sbl = new StarBlockList( i, this );
            if( !sbl ) {
                qDebug() << "NULL starBlockList. Expect trouble!";
            }
            m_starBlockList.append( sbl );
        }
        m_zoomMagLimit = 0.06;
    }

    return fileOpened;
}


StarObject *DeepStarComponent::findByHDIndex( int HDnum ) {
    // Currently, we only handle HD catalog indexes
    return m_CatalogNumber.value( HDnum, NULL ); // TODO: Maybe, make this more general.
}

// This uses the main star index for looking up nearby stars but then
// filters out objects with the generic name "star".  We could easily
// build an index for just the named stars which would make this go
// much faster still.  -jbb
//


SkyObject* DeepStarComponent::objectNearest( SkyPoint *p, double &maxrad )
{
    StarObject *oBest = 0;

    if( !fileOpened )
        return NULL;

    m_skyMesh->index( p, maxrad + 1.0, OBJ_NEAREST_BUF);

    MeshIterator region( m_skyMesh, OBJ_NEAREST_BUF );

    while ( region.hasNext() ) {
        Trixel currentRegion = region.next();
        for( int i = 0; i < m_starBlockList.at( currentRegion )->getBlockCount(); ++i ) {
            StarBlock *block = m_starBlockList.at( currentRegion )->block( i );
            for( int j = 0; j < block->getStarCount(); ++j ) {
                StarObject* star =  block->star( j );
                if( !star ) continue;
                if ( star->mag() > m_zoomMagLimit ) continue;
                
                double r = star->angularDistanceTo( p ).Degrees();
                if ( r < maxrad ) {
                    oBest = star;
                    maxrad = r;
                }
            }
        }
    }

    // TODO: What if we are looking around a point that's not on
    // screen? objectNearest() will need to keep on filling up all
    // trixels around the SkyPoint to find the best match in case it
    // has not been found. Ideally, this should be implemented in a
    // different method and should be called after all other
    // candidates (eg: DeepSkyObject::objectNearest()) have been
    // called.
    
    return oBest;
}

bool DeepStarComponent::starsInAperture( QList<StarObject *> &list, const SkyPoint &center, float radius, float maglim )
{

    if( maglim < triggerMag )
        return false;

    // For DeepStarComponents, whether we use ra0() and dec0(), or
    // ra() and dec(), should not matter, because the stars are
    // repeated in all trixels that they will pass through, although
    // the factuality of this statement needs to be verified

    // Ensure that we have deprecessed the (RA, Dec) to (RA0, Dec0)
    Q_ASSERT( center.ra0().Degrees() >= 0.0 );
    Q_ASSERT( center.dec0().Degrees() <= 90.0 );

    m_skyMesh->intersect( center.ra0().Degrees(), center.dec0().Degrees(), radius, (BufNum) OBJ_NEAREST_BUF );

    MeshIterator region( m_skyMesh, OBJ_NEAREST_BUF );

    if( maglim < -28 )
        maglim = m_FaintMagnitude;

    while ( region.hasNext() ) {
        Trixel currentRegion = region.next();
        // FIXME: Build a better way to iterate over all stars.
        // Ideally, StarBlockList should have such a facility.
        StarBlockList *sbl = m_starBlockList[ currentRegion ];
        sbl->fillToMag( maglim );
        for( int i = 0; i < sbl->getBlockCount(); ++i ) {
            StarBlock *block = sbl->block( i );
            for( int j = 0; j < block->getStarCount(); ++j ) {
                StarObject *star = block->star( j );
                if( star->mag() > maglim )
                    break; // Stars are organized by magnitude, so this should work
                if( star->angularDistanceTo( &center ).Degrees() <= radius )
                    list.append( star );
            }
        }
    }

    return true;
}

void DeepStarComponent::byteSwap( deepStarData *stardata ) {
    stardata->RA = bswap_32( stardata->RA );
    stardata->Dec = bswap_32( stardata->Dec );
    stardata->dRA = bswap_16( stardata->dRA );
    stardata->dDec = bswap_16( stardata->dDec );
    stardata->B = bswap_16( stardata->B );
    stardata->V = bswap_16( stardata->V );
}

void DeepStarComponent::byteSwap( starData *stardata ) {
    stardata->RA = bswap_32( stardata->RA );
    stardata->Dec = bswap_32( stardata->Dec );
    stardata->dRA = bswap_32( stardata->dRA );
    stardata->dDec = bswap_32( stardata->dDec );
    stardata->parallax = bswap_32( stardata->parallax );
    stardata->HD = bswap_32( stardata->HD );
    stardata->mag = bswap_16( stardata->mag );
    stardata->bv_index = bswap_16( stardata->bv_index );
}

bool DeepStarComponent::verifySBLIntegrity() {
    float faintMag = -5.0;
    bool integrity = true;
    for(Trixel trixel = 0; trixel < (unsigned int)m_skyMesh->size(); ++trixel) {
        for(int i = 0; i < m_starBlockList[ trixel ]->getBlockCount(); ++i) {
            StarBlock *block = m_starBlockList[ trixel ]->block( i );
            if( i == 0 )
                faintMag = block->getBrightMag();
            // NOTE: Assumes 2 decimal places in magnitude field. TODO: Change if it ever does change
            if( block->getBrightMag() != faintMag && ( block->getBrightMag() - faintMag ) > 0.5) {
                qDebug() << "Trixel " << trixel << ": ERROR: faintMag of prev block = " << faintMag 
                         << ", brightMag of block #" << i << " = " << block->getBrightMag();
                integrity = false;
            }
            if( i > 1 && ( !block->prev ) )
                qDebug() << "Trixel " << trixel << ": ERROR: Block" << i << "is unlinked in LRU Cache";
            if( block->prev && block->prev->parent == m_starBlockList[ trixel ] 
                && block->prev != m_starBlockList[ trixel ]->block( i - 1 ) ) {
                qDebug() << "Trixel " << trixel << ": ERROR: SBF LRU Cache linked list seems to be broken at before block " << i << endl;
                integrity = false;
            }
            faintMag = block->getFaintMag();
        }
    }
    return integrity;
}
