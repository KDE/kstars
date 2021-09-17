/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "deepstaritem.h"

#include "deepstarcomponent.h"
#include "labelsitem.h"
#include "Options.h"
#include "rootnode.h"
#include "skymesh.h"
#include "starblock.h"
#include "starblockfactory.h"
#include "starblocklist.h"
#include "starcomponent.h"
#include "htmesh/MeshIterator.h"
#include "projections/projector.h"
#include "skynodes/pointsourcenode.h"
#include "skynodes/trixelnode.h"

DeepStarItem::DeepStarItem(DeepStarComponent *deepStarComp, RootNode *rootNode)
    : SkyItem(LabelsItem::label_t::NO_LABEL, rootNode), m_deepStarComp(deepStarComp),
      m_staticStars(deepStarComp->staticStars)
{
    m_starBlockList = &m_deepStarComp->m_starBlockList;

    if (m_staticStars)
    {
        for (int c = 0; c < m_starBlockList->size(); ++c)
        {
            TrixelNode *trixel = new TrixelNode(m_starBlockList->at(c)->getTrixel());
            appendChildNode(trixel);
            int blockCount = m_starBlockList->at(c)->getBlockCount();

            for (int i = 0; i < blockCount; ++i)
            {
                std::shared_ptr<StarBlock> block = m_starBlockList->at(c)->block(i);
                //            qDebug() << "---> Drawing stars from block " << i << " of trixel " <<
                //                currentRegion << ". SB has " << block->getStarCount() << " stars" << endl;
                int starCount = block->getStarCount();
                for (int j = 0; j < starCount; j++)
                {
                    StarObject *star = &(block->star(j)->star);

                    if (star)
                    {
                        trixel->m_nodes.append(QPair<SkyObject *, SkyNode *>(star, 0));
                    }
                }
            }
        }
    }

    m_skyMesh          = SkyMesh::Instance();
    m_StarBlockFactory = StarBlockFactory::Instance();
}

void DeepStarItem::update()
{
    if (m_staticStars) // dynamic stars under construction
    {
        SkyMapLite *map   = SkyMapLite::Instance();
        KStarsData *data  = KStarsData::Instance();
        UpdateID updateID = data->updateID();

        //FIXME_FOV -- maybe not clamp like that...
        float radius = map->projector()->fov();
        if (radius > 90.0)
            radius = 90.0;

        if (m_skyMesh != SkyMesh::Instance() && m_skyMesh->inDraw())
        {
            printf("Warning: aborting concurrent DeepStarComponent::draw()");
        }
        bool checkSlewing = (map->isSlewing() && Options::hideOnSlew());

        //shortcuts to inform whether to draw different objects
        bool hideFaintStars(checkSlewing && Options::hideStars());
        double hideStarsMag = Options::magLimitHideStar();

        //adjust maglimit for ZoomLevel
        //    double lgmin = log10(MINZOOM);
        //    double lgmax = log10(MAXZOOM);
        //    double lgz = log10(Options::zoomFactor());
        // TODO: Enable hiding of faint stars

        float maglim = StarComponent::zoomMagnitudeLimit();

        if (maglim < m_deepStarComp->triggerMag || !m_staticStars)
        {
            hide();
            return;
        }
        else
        {
            show();
        }

        //float m_zoomMagLimit = maglim;

        m_skyMesh->inDraw(true);

        SkyPoint *focus = map->focus();
        m_skyMesh->aperture(focus, radius + 1.0, DRAW_BUF); // divide by 2 for testing

        MeshIterator region(m_skyMesh, DRAW_BUF);

        // If we are to hide the fainter stars (eg: while slewing), we set the magnitude limit to hideStarsMag.
        if (hideFaintStars && maglim > hideStarsMag)
            maglim = hideStarsMag;

        StarBlockFactory *m_StarBlockFactory = StarBlockFactory::Instance();
        //    m_StarBlockFactory->drawID = m_skyMesh->drawID();
        //    qDebug() << "Mesh size = " << m_skyMesh->size() << "; drawID = " << m_skyMesh->drawID();

        /*t_dynamicLoad = 0;
        t_updateCache = 0;
        t_drawUnnamed = 0;*/

        //visibleStarCount = 0;

        // Mark used blocks in the LRU Cache. Not required for static stars
        if (!m_staticStars)
        {
            //Under construction
            //            while( region.hasNext() ) {
            //                Trixel currentRegion = region.next();
            //                for( int i = 0; i < m_starBlockList->at( currentRegion )->getBlockCount(); ++i ) {
            //                    StarBlock *prevBlock = ( ( i >= 1 ) ? m_starBlockList->at( currentRegion )->block( i - 1 ) : nullptr );
            //                    StarBlock *block = m_starBlockList->at( currentRegion )->block( i );

            //                    if( i == 0  &&  !m_StarBlockFactory->markFirst( block ) )
            //                        qDebug() << "markFirst failed in trixel" << currentRegion;
            //                    if( i > 0   &&  !m_StarBlockFactory->markNext( prevBlock, block ) )
            //                        qDebug() << "markNext failed in trixel" << currentRegion << "while marking block" << i;
            //                    if( i < m_starBlockList->at( currentRegion )->getBlockCount()
            //                            && m_starBlockList->at( currentRegion )->block( i )->getFaintMag() < maglim )
            //                        break;
            //                }
            //            }
            //            //t_updateCache = t.restart();
            //            region.reset();
        }

        m_StarBlockFactory->drawID = m_skyMesh->drawID();

        int regionID = -1;
        if (region.hasNext())
        {
            regionID = region.next();
        }

        int trixelID = 0;

        QSGNode *firstTrixel = firstChild();
        TrixelNode *trixel   = static_cast<TrixelNode *>(firstTrixel);

        while (trixel != 0)
        {
            if (m_staticStars)
            {
                const Projector *projector = SkyMapLite::Instance()->projector();
                double delLim              = SkyMapLite::deleteLimit();

                if (trixelID != regionID)
                {
                    trixel->hide();

                    if (trixel->hideCount() > delLim)
                    {
                        QLinkedList<QPair<SkyObject *, SkyNode *>>::iterator i = trixel->m_nodes.begin();

                        while (i != trixel->m_nodes.end())
                        {
                            SkyNode *node = (*i).second;
                            if (node)
                            {
                                trixel->removeChildNode(node);
                                delete node;

                                *i = QPair<SkyObject *, SkyNode *>((*i).first, 0);
                            }
                            ++i;
                        }
                    }

                    trixel = static_cast<TrixelNode *>(trixel->nextSibling());
                    ++trixelID;
                    continue;
                }
                else
                {
                    trixel->show();

                    if (region.hasNext())
                    {
                        regionID = region.next();
                    }

                    QLinkedList<QPair<SkyObject *, SkyNode *>>::iterator i = (&trixel->m_nodes)->begin();

                    while (i != (&trixel->m_nodes)->end())
                    {
                        bool hide     = false;
                        bool hideSlew = false;

                        bool drawLabel = false;

                        StarObject *starObj = static_cast<StarObject *>((*i).first);
                        SkyNode *node       = (*i).second;

                        int mag = starObj->mag();

                        // break loop if maglim is reached
                        if (mag > maglim)
                            hide = true;
                        if (hideFaintStars && hideStarsMag)
                            hideSlew = true;
                        if (starObj->updateID != KStarsData::Instance()->updateID())
                            starObj->JITupdate();

                        if (node)
                        {
                            if (node->hideCount() > delLim || hide)
                            {
                                trixel->removeChildNode(node);
                                delete node;
                                *i = QPair<SkyObject *, SkyNode *>((*i).first, 0);
                            }
                            else
                            {
                                if (!hideSlew)
                                {
                                    node->update(drawLabel);
                                }
                                else
                                {
                                    node->hide();
                                }
                            }
                        }
                        else
                        {
                            if (!hide && !hideSlew && projector->checkVisibility(starObj))
                            {
                                QPointF pos;

                                bool visible = false;
                                pos          = projector->toScreen(starObj, true, &visible);
                                if (visible && projector->onScreen(pos))
                                {
                                    PointSourceNode *point =
                                        new PointSourceNode(starObj, rootNode(), LabelsItem::label_t::STAR_LABEL,
                                                            starObj->spchar(), starObj->mag(), trixelID);
                                    trixel->appendChildNode(point);

                                    *i = QPair<SkyObject *, SkyNode *>((*i).first, static_cast<SkyNode *>(point));
                                    point->updatePos(pos, drawLabel);
                                }
                            }
                        }
                        ++i;
                    }
                }
            }
            else if (false)
            {
                //Dynamic stars are under construction
                if (!m_staticStars && !m_starBlockList->at(regionID)->fillToMag(maglim) &&
                    maglim <= m_deepStarComp->m_FaintMagnitude * (1 - 1.5 / 16))
                {
                    qDebug() << "SBL::fillToMag( " << maglim << " ) failed for trixel " << regionID << " !" << endl;
                }

                for (int i = 0; i < m_starBlockList->at(regionID)->getBlockCount(); ++i)
                {
                    bool hide = false;

                    std::shared_ptr<StarBlock> block = m_starBlockList->at(regionID)->block(i);

                    for (int j = 0; j < block->getStarCount(); j++)
                    {
                        StarNode *star         = block->star(j);
                        StarObject *curStar    = &(star->star);
                        PointSourceNode *point = star->starNode;

                        if (curStar->updateID != updateID)
                            curStar->JITupdate();

                        float mag = curStar->mag();

                        if (!hide)
                        {
                            if (mag > maglim)
                                hide = true;
                        }

                        if (!hide)
                        {
                            if (!point)
                            {
                                star->starNode = new PointSourceNode(curStar, rootNode(), LabelsItem::label_t::NO_LABEL,
                                                                     curStar->spchar(), curStar->mag(), regionID);
                                point          = star->starNode;
                                trixel->appendChildNode(point);
                            }
                            //point->setSizeMagLim(m_zoomMagLimit);
                            point->update();
                        }
                        else
                        {
                            if (point)
                                point->hide();
                        }
                    }
                }
            }
            trixel = static_cast<TrixelNode *>(trixel->nextSibling());
            trixelID++;
        }
        m_skyMesh->inDraw(false);
    }
}
