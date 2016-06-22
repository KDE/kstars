/** *************************************************************************
                          staritem.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 15/06/2016
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
#include "staritem.h"
#include "deepstaritem.h"

#include "starcomponent.h"
#include "starblockfactory.h"
#include "skymesh.h"
#include "rootnode.h"

StarItem::StarItem(StarComponent *starComp, RootNode *rootNode)
    :SkyItem(LabelsItem::label_t::STAR_LABEL, rootNode), m_starComp(starComp)
    ,m_starLabels(rootNode->labelsItem()->getLabelNode(labelType())), m_stars(new SkyOpacityNode),
      m_deepStars(new SkyOpacityNode)
{
    StarIndex *trixels = m_starComp->m_starIndex;
    appendChildNode(m_stars);

    //Test
    Options::setShowStarMagnitudes(false);
    Options::setShowStarNames(true);

    for(int i = 0; i < trixels->size(); ++i) {
        StarList *skyList = trixels->at(i);
        TrixelNode *trixel = new TrixelNode;
        m_stars->appendChildNode(trixel);

        for(int c = 0; c < skyList->size(); ++c) {
            StarObject *star = skyList->at(c);
            if(star) {
                PointSourceNode *point = new PointSourceNode(star, rootNode, LabelsItem::label_t::STAR_LABEL, star->spchar(),
                                                             star->mag(), i);
                trixel->appendChildNode(point);
            }
        }
    }

    appendChildNode(m_deepStars);

    QVector<DeepStarComponent*> deepStars = m_starComp->m_DeepStarComponents;
    int deepSize = deepStars.size();
    for(int i = 0; i < deepSize; ++i) {
        DeepStarItem *deepStar = new DeepStarItem(deepStars[i], rootNode);
        rootNode->removeChildNode(deepStar);
        m_deepStars->appendChildNode(deepStar);
    }

    m_skyMesh = SkyMesh::Instance();
    m_StarBlockFactory = StarBlockFactory::Instance();
}

void StarItem::update() {
    if( !m_starComp->selected() ) {
        hide();
        return;
    }
    show();

    SkyMapLite *map             = SkyMapLite::Instance();
    KStarsData* data        = KStarsData::Instance();

    bool checkSlewing = ( map->isSlewing() && Options::hideOnSlew() );
    bool hideLabel = checkSlewing || !( Options::showStarMagnitudes() || Options::showStarNames() );
    if(hideLabel) hideLabels();

    //shortcuts to inform whether to draw different objects
    bool hideFaintStars = checkSlewing && Options::hideStars();
    double hideStarsMag = Options::magLimitHideStar();
    bool reIndex = m_starComp->reindex( data->updateNum() );

    double lgmin = log10(MINZOOM);
    double lgmax = log10(MAXZOOM);
    double lgz   = log10(Options::zoomFactor());

    double maglim;
    double m_zoomMagLimit; //Check it later. Needed for labels
    m_zoomMagLimit = maglim = StarComponent::zoomMagnitudeLimit();
    map->setSizeMagLim(m_zoomMagLimit);

    double labelMagLim = Options::starLabelDensity() / 5.0;
    labelMagLim += ( 12.0 - labelMagLim ) * ( lgz - lgmin) / (lgmax - lgmin );
    if( labelMagLim > 8.0 )
        labelMagLim = 8.0;

    //Calculate sizeMagLim
    // Old formula:
    //    float sizeMagLim = ( 2.000 + 2.444 * Options::memUsage() / 10.0 ) * ( lgz - lgmin ) + 5.8;

    // Using the maglim to compute the sizes of stars reduces
    // discernability between brighter and fainter stars at high zoom
    // levels. To fix that, we use an "arbitrary" constant in place of
    // the variable star density.
    // Not using this formula now.
    //    float sizeMagLim = 4.444 * ( lgz - lgmin ) + 5.0;

    /*float sizeMagLim = zoomMagnitudeLimit();
    if( sizeMagLim > faintMagnitude() * ( 1 - 1.5/16 ) )
        sizeMagLim = faintMagnitude() * ( 1 - 1.5/16 );
    skyp->setSizeMagLimit(sizeMagLim);*/

    //Loop for drawing star images

    float radius = map->projector()->fov();
    if ( radius > 90.0 ) radius = 90.0;

    m_skyMesh->inDraw( true );

    SkyPoint* focus = map->focus();
    m_skyMesh->aperture( focus, radius + 1.0, DRAW_BUF ); // divide by 2 for testing

    MeshIterator region(m_skyMesh, DRAW_BUF);

    // If we are hiding faint stars, then maglim is really the brighter of hideStarsMag and maglim
    if( hideFaintStars && maglim > hideStarsMag )
        maglim = hideStarsMag;

    m_StarBlockFactory->drawID = m_skyMesh->drawID();

    int regionID = -1;
    if(region.hasNext()) {
        regionID = region.next();
    }

    int trixelID = 0;

    QSGNode *firstTrixel = m_stars->firstChild();
    TrixelNode *trixel = static_cast<TrixelNode *>(firstTrixel);

    QSGNode *firstLabel = m_starLabels->firstChild();
    TrixelNode *label = static_cast<TrixelNode *>(firstLabel);

    StarIndex *index = m_starComp->m_starIndex;

    if(reIndex) rootNode()->labelsItem()->deleteLabels(labelType());

    while( trixel != 0 ) {
        //All stars were reindexed so recreate the trixel nodes again
        if(reIndex) {
            StarList *skyList = index->at(trixelID);

            QSGNode *n = trixel->firstChild();
            while( n != 0 ) {
                QSGNode *c = n;
                n = n->nextSibling();

                //Delete node
                trixel->removeChildNode(c);
                delete c;
            }

            for(int c = 0; c < skyList->size(); ++c) {
                StarObject *star = skyList->at(c);
                if(star) {
                    PointSourceNode *point = new PointSourceNode(star, rootNode(), LabelsItem::label_t::STAR_LABEL, star->spchar(),
                                                                 star->mag(), c);
                    trixel->appendChildNode(point);
                }
            }
        }

        if(trixelID != regionID) {
            trixel->hide();
            label->hide();

        } else {
            trixel->show();
            label->show();

            if(region.hasNext()) {
                regionID = region.next();
            }

            QSGNode *n = trixel->firstChild();

            bool hide = false;

            while(n != 0) {
                PointSourceNode *point = static_cast<PointSourceNode *>(n);
                n = n->nextSibling();

                StarObject *curStar = static_cast<StarObject *>(point->skyObject());
                float mag = curStar->mag();

                bool drawLabel = false;
                // break loop if maglim is reached
                if(!hide) {
                    if ( mag > maglim ) hide = true;
                    if(!(hideLabel || mag > labelMagLim)) drawLabel = true;
                }

                if(!hide) {
                    if ( curStar->updateID != KStarsData::Instance()->updateID() )
                        curStar->JITupdate();
                    point->SkyNode::update(drawLabel);
                } else {
                    point->hide();
                }
            }
        }
        trixel = static_cast<TrixelNode *>(trixel->nextSibling());
        label = static_cast<TrixelNode *>(label->nextSibling());

        trixelID++;
    }

    // Draw focusStar if not null
    /*if( focusStar ) {
        if ( focusStar->updateID != updateID )
            focusStar->JITupdate();
        float mag = focusStar->mag();
        skyp->drawPointSource(focusStar, mag, focusStar->spchar() );
    }*/

    // Now draw each of our DeepStarComponents
    QSGNode *deep = m_deepStars->firstChild();
    while( deep != 0 ) {
        DeepStarItem *deepStars = static_cast<DeepStarItem *>(deep);
        deep = deep->nextSibling();
        deepStars->update();
    }

    m_skyMesh->inDraw( false );
}

