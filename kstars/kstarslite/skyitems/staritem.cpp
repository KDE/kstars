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

#include "starcomponent.h"
#include "starblockfactory.h"
#include "skymesh.h"
#include "rootnode.h"

StarItem::StarItem(StarComponent *starComp, RootNode *rootNode)
    :SkyItem(LabelsItem::label_t::STAR_LABEL, rootNode), m_starComp(starComp)
    ,m_starLabels(rootNode->labelsItem()->getLabelNode(labelType()))
{
    StarIndex *trixels = m_starComp->starIndex();

    //Test
    Options::setShowStarMagnitudes(false);
    Options::setShowStarNames(true);

    for(int i = 0; i < trixels->size(); ++i) {
        StarList *skyList = trixels->at(i);
        TrixelNode *trixel = new TrixelNode;
        appendChildNode(trixel);

        for(int c = 0; c < skyList->size(); ++c) {
            StarObject *star = skyList->at(c);
            if(star) {
                PointSourceNode *point = new PointSourceNode(star, rootNode, LabelsItem::label_t::STAR_LABEL, star->spchar(),
                                                             star->mag(), i);
                trixel->appendChildNode(point);
            }
        }
    }

    m_skyMesh = SkyMesh::Instance();
    m_StarBlockFactory = StarBlockFactory::Instance();
}

void StarItem::update() {

    if( !m_starComp->selected() ) {
        hide();
        return;
    }

    SkyMapLite *map             = SkyMapLite::Instance();
    const Projector *proj   = map->projector();
    KStarsData* data        = KStarsData::Instance();
    UpdateID updateID       = data->updateID();

    bool checkSlewing = ( map->isSlewing() && Options::hideOnSlew() );
    bool hideLabels = checkSlewing || !( Options::showStarMagnitudes() || Options::showStarNames() );

    //shortcuts to inform whether to draw different objects
    bool hideFaintStars = checkSlewing && Options::hideStars();
    double hideStarsMag = Options::magLimitHideStar();
    m_starComp->reindex( data->updateNum() );

    double lgmin = log10(MINZOOM);
    double lgmax = log10(MAXZOOM);
    double lgz   = log10(Options::zoomFactor());

    double maglim;
    double m_zoomMagLimit; //Check it later. Needed for labels
    m_zoomMagLimit = maglim = m_starComp->zoomMagnitudeLimit();

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

    MeshIterator region(m_skyMesh, DRAW_BUF);

    // If we are hiding faint stars, then maglim is really the brighter of hideStarsMag and maglim
    if( hideFaintStars && maglim > hideStarsMag )
        maglim = hideStarsMag;

    m_magLim = maglim;

    m_StarBlockFactory->drawID = m_skyMesh->drawID();

    int nTrixels = 0;

    int regionID = -1;
    if(region.hasNext()) {
        regionID = region.next();
    }

    int trixelID = 0;
    //++nTrixels;
    QSGNode *firstTrixel = firstChild();
    TrixelNode *trixel = static_cast<TrixelNode *>(firstTrixel);

    QSGNode *firstLabel = m_starLabels->firstChild();
    TrixelNode *label = static_cast<TrixelNode *>(firstLabel);

    while( trixel != 0 ) {
        if(trixelID != regionID) {
            trixel->hide();
            label->hide();

            trixel = static_cast<TrixelNode *>(trixel->nextSibling());
            label = static_cast<TrixelNode *>(label->nextSibling());

            trixelID++;

            continue;
        } else {
            trixel->show();
            label->show();

            if(region.hasNext()) {
                regionID = region.next();
            }
        }

        QSGNode *n = trixel->firstChild();

        while(n != 0) {
            PointSourceNode *point = static_cast<PointSourceNode *>(n);
            n = n->nextSibling();

            StarObject *curStar = static_cast<StarObject *>(point->skyObject());
            float mag = curStar->mag();

            bool hide = false;

            // break loop if maglim is reached
            if ( mag > m_magLim ) hide = true;
            bool drawLabel = false;
            if(mag < labelMagLim) drawLabel = true;

            if(!hide) {
                if ( curStar->updateID != KStarsData::Instance()->updateID() )
                    curStar->JITupdate();
                point->setSizeMagLim(m_zoomMagLimit);
                point->SkyNode::update(drawLabel);
            } else {
                point->hide();
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
    /*for( int i =0; i < m_DeepStarComponents.size(); ++i ) {
        m_DeepStarComponents.at( i )->draw( skyp );
    }*/
}

