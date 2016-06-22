/** *************************************************************************
                          deepstaritem.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 17/06/2016
    copyright            : (C) 2016 by Artem Fedoskin
    email                : afedoskin3@gmail.com
 ***************************************************************************/
/** *************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "Options.h"
#include "projections/projector.h"

#include "skynodes/pointsourcenode.h"
#include "labelsitem.h"
#include "deepstaritem.h"

#include "deepstarcomponent.h"
#include "starcomponent.h"

#include "starblockfactory.h"
#include "skymesh.h"
#include "rootnode.h"

DeepStarItem::DeepStarItem(DeepStarComponent *deepStarComp, RootNode *rootNode)
    :SkyItem(LabelsItem::label_t::NO_LABEL, rootNode), m_deepStarComp(deepStarComp),
      m_staticStars(deepStarComp->staticStars)
{
    m_starBlockList = &m_deepStarComp->m_starBlockList;

    //Test
    Options::setShowStarMagnitudes(false);
    Options::setShowStarNames(true);

    for(int c = 0; c < m_starBlockList->size(); ++c) {
        TrixelNode *trixel = new TrixelNode;
        appendChildNode(trixel);
        if(m_staticStars) {
            int blockCount = m_starBlockList->at( c )->getBlockCount();

            for( int i = 0; i < blockCount; ++i ) {
                StarBlock *block = m_starBlockList->at( c )->block( i );
                //            qDebug() << "---> Drawing stars from block " << i << " of trixel " <<
                //                currentRegion << ". SB has " << block->getStarCount() << " stars" << endl;
                int starCount = block->getStarCount();
                for( int j = 0; j < starCount; j++ ) {

                    StarObject *star = &(block->star( j )->star);

                    if(star) {
                        PointSourceNode *point = new PointSourceNode(star, rootNode, LabelsItem::label_t::NO_LABEL, star->spchar(),
                                                                     star->mag(), i);
                        trixel->appendChildNode(point);
                    }
                }
            }
        }
    }

    m_skyMesh = SkyMesh::Instance();
    m_StarBlockFactory = StarBlockFactory::Instance();
}

void DeepStarItem::update() {
#ifndef KSTARS_LITE
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
#endif

    if(m_staticStars) { // dynamic stars under construction
        SkyMapLite *map             = SkyMapLite::Instance();
        KStarsData* data        = KStarsData::Instance();
        UpdateID updateID       = data->updateID();

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

        if( maglim < m_deepStarComp->triggerMag || !m_staticStars ) {
            hide();
            return;
        } else {
            show();
        }

        float m_zoomMagLimit = maglim;

        m_skyMesh->inDraw( true );

        SkyPoint* focus = map->focus();
        m_skyMesh->aperture( focus, radius + 1.0, DRAW_BUF ); // divide by 2 for testing

        MeshIterator region(m_skyMesh, DRAW_BUF);

        // If we are to hide the fainter stars (eg: while slewing), we set the magnitude limit to hideStarsMag.
        if( hideFaintStars && maglim > hideStarsMag )
            maglim = hideStarsMag;

        StarBlockFactory *m_StarBlockFactory = StarBlockFactory::Instance();
        //    m_StarBlockFactory->drawID = m_skyMesh->drawID();
        //    qDebug() << "Mesh size = " << m_skyMesh->size() << "; drawID = " << m_skyMesh->drawID();

        /*t_dynamicLoad = 0;
        t_updateCache = 0;
        t_drawUnnamed = 0;*/

        //visibleStarCount = 0;

        // Mark used blocks in the LRU Cache. Not required for static stars
        if( !m_staticStars ) {
            while( region.hasNext() ) {
                Trixel currentRegion = region.next();
                for( int i = 0; i < m_starBlockList->at( currentRegion )->getBlockCount(); ++i ) {
                    StarBlock *prevBlock = ( ( i >= 1 ) ? m_starBlockList->at( currentRegion )->block( i - 1 ) : NULL );
                    StarBlock *block = m_starBlockList->at( currentRegion )->block( i );

                    if( i == 0  &&  !m_StarBlockFactory->markFirst( block ) )
                        qDebug() << "markFirst failed in trixel" << currentRegion;
                    if( i > 0   &&  !m_StarBlockFactory->markNext( prevBlock, block ) )
                        qDebug() << "markNext failed in trixel" << currentRegion << "while marking block" << i;
                    if( i < m_starBlockList->at( currentRegion )->getBlockCount()
                            && m_starBlockList->at( currentRegion )->block( i )->getFaintMag() < maglim )
                        break;
                }
            }
            //t_updateCache = t.restart();
            region.reset();
        }

        m_StarBlockFactory->drawID = m_skyMesh->drawID();

        int regionID = -1;
        if(region.hasNext()) {
            regionID = region.next();
        }

        int trixelID = 0;

        QSGNode *firstTrixel = firstChild();
        TrixelNode *trixel = static_cast<TrixelNode *>(firstTrixel);

        while( trixel != 0 ) {
            if(trixelID != regionID) {
                trixel->hide();

                trixel = static_cast<TrixelNode *>(trixel->nextSibling());

                trixelID++;

                continue;
            } else {
                trixel->show();

                if(region.hasNext()) {
                    regionID = region.next();

                }
            }

            if(m_staticStars) {

                QSGNode *n = trixel->firstChild();

                bool hide = false;

                while(n != 0) {
                    PointSourceNode *point = static_cast<PointSourceNode *>(n);
                    n = n->nextSibling();

                    StarObject *curStar = static_cast<StarObject *>(point->skyObject());
                    float mag = curStar->mag();

                    // break loop if maglim is reached
                    if(!hide) {
                        if ( mag > maglim ) hide = true;
                    }

                    if(!hide) {
                        if ( curStar->updateID != KStarsData::Instance()->updateID() )
                            curStar->JITupdate();
                        //point->setSizeMagLim(m_zoomMagLimit);
                        point->update();
                    } else {
                        point->hide();
                    }
                }
            } else if(false) {
                if( !m_staticStars && !m_starBlockList->at( regionID )->fillToMag( maglim ) && maglim <= m_deepStarComp->m_FaintMagnitude * ( 1 - 1.5/16 ) ) {
                    qDebug() << "SBL::fillToMag( " << maglim << " ) failed for trixel "
                             << regionID << " !"<< endl;
                }

                for( int i = 0; i < m_starBlockList->at( regionID )->getBlockCount(); ++i ) {

                    bool hide = false;

                    StarBlock *block = m_starBlockList->at( regionID )->block( i );
                    for( int j = 0; j < block->getStarCount(); j++ ) {

                        StarNode *star = block->star( j );
                        StarObject *curStar = &(star->star);
                        PointSourceNode *point = star->starNode;

                        if ( curStar->updateID != updateID )
                            curStar->JITupdate();

                        float mag = curStar->mag();

                        if(!hide) {
                            if ( mag > maglim ) hide = true;
                        }

                        if(!hide) {
                            if(!point) {
                                star->starNode = new PointSourceNode(curStar, rootNode(), LabelsItem::label_t::NO_LABEL, curStar->spchar(),
                                                                     curStar->mag(), regionID);
                                point = star->starNode;
                                trixel->appendChildNode(point);
                            }
                            //point->setSizeMagLim(m_zoomMagLimit);
                            point->update();

                        } else {
                            if(point) point->hide();
                        }
                    }
                }
            }
            trixel = static_cast<TrixelNode *>(trixel->nextSibling());

            trixelID++;
        }
        m_skyMesh->inDraw( false );
    }
}

