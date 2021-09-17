/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "staritem.h"

#include "deepstaritem.h"
#include "labelsitem.h"
#include "Options.h"
#include "rootnode.h"
#include "skymesh.h"
#include "skyopacitynode.h"
#include "starblockfactory.h"
#include "starcomponent.h"
#include "htmesh/MeshIterator.h"
#include "projections/projector.h"
#include "skynodes/pointsourcenode.h"
#include "skynodes/trixelnode.h"

#include <QLinkedList>

StarItem::StarItem(StarComponent *starComp, RootNode *rootNode)
    : SkyItem(LabelsItem::label_t::STAR_LABEL, rootNode), m_starComp(starComp), m_stars(new SkyOpacityNode),
      m_deepStars(new SkyOpacityNode), m_starLabels(rootNode->labelsItem()->getLabelNode(labelType()))

{
    StarIndex *trixels = m_starComp->m_starIndex.get();

    appendChildNode(m_stars);

    //Test
    Options::setShowStarMagnitudes(false);
    Options::setShowStarNames(true);

    for (int i = 0; i < trixels->size(); ++i)
    {
        StarList *skyList  = trixels->at(i);
        TrixelNode *trixel = new TrixelNode(i);
        m_stars->appendChildNode(trixel);

        for (int c = 0; c < skyList->size(); ++c)
        {
            StarObject *star = skyList->at(c);
            if (star)
            {
                trixel->m_nodes.append(QPair<SkyObject *, SkyNode *>(star, 0));
            }
        }
    }

    appendChildNode(m_deepStars);

    QVector<DeepStarComponent *> deepStars = m_starComp->m_DeepStarComponents;
    int deepSize                           = deepStars.size();

    for (int i = 0; i < deepSize; ++i)
    {
        DeepStarItem *deepStar = new DeepStarItem(deepStars[i], rootNode);
        rootNode->removeChildNode(deepStar);
        m_deepStars->appendChildNode(deepStar);
    }

    m_skyMesh          = SkyMesh::Instance();
    m_StarBlockFactory = StarBlockFactory::Instance();
}

void StarItem::update()
{
    if (!m_starComp->selected())
    {
        hide();
        return;
    }
    show();

    SkyMapLite *map  = SkyMapLite::Instance();
    KStarsData *data = KStarsData::Instance();

    bool checkSlewing = (map->isSlewing() && Options::hideOnSlew());
    bool hideLabel    = checkSlewing || !(Options::showStarMagnitudes() || Options::showStarNames());
    if (hideLabel)
        hideLabels();

    //shortcuts to inform whether to draw different objects
    bool hideFaintStars = checkSlewing && Options::hideStars();
    double hideStarsMag = Options::magLimitHideStar();
    bool reIndex        = m_starComp->reindex(data->updateNum());

    double lgmin = log10(MINZOOM);
    double lgmax = log10(MAXZOOM);
    double lgz   = log10(Options::zoomFactor());

    double maglim;
    double m_zoomMagLimit; //Check it later. Needed for labels
    m_zoomMagLimit = maglim = StarComponent::zoomMagnitudeLimit();
    map->setSizeMagLim(m_zoomMagLimit);

    double labelMagLim = Options::starLabelDensity() / 5.0;
    labelMagLim += (12.0 - labelMagLim) * (lgz - lgmin) / (lgmax - lgmin);
    if (labelMagLim > 8.0)
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
    if (radius > 90.0)
        radius = 90.0;

    m_skyMesh->inDraw(true);

    SkyPoint *focus = map->focus();
    m_skyMesh->aperture(focus, radius + 1.0, DRAW_BUF); // divide by 2 for testing

    MeshIterator region(m_skyMesh, DRAW_BUF);

    // If we are hiding faint stars, then maglim is really the brighter of hideStarsMag and maglim
    if (hideFaintStars && maglim > hideStarsMag)
        maglim = hideStarsMag;

    m_StarBlockFactory->drawID = m_skyMesh->drawID();

    int regionID = -1;
    if (region.hasNext())
    {
        regionID = region.next();
    }

    int trixelID = 0;

    QSGNode *firstTrixel = m_stars->firstChild();
    TrixelNode *trixel   = static_cast<TrixelNode *>(firstTrixel);

    QSGNode *firstLabel = m_starLabels->firstChild();
    TrixelNode *label   = static_cast<TrixelNode *>(firstLabel);

    StarIndex *index = m_starComp->m_starIndex.get();

    if (reIndex)
        rootNode()->labelsItem()->deleteLabels(labelType());

    const Projector *projector = SkyMapLite::Instance()->projector();

    double delLim = SkyMapLite::deleteLimit();

    while (trixel != 0)
    {
        if (reIndex)
        {
            StarList *skyList = index->at(trixelID);

            QSGNode *n = trixel->firstChild();

            //Delete nodes
            while (n != 0)
            {
                QSGNode *c = n;
                n          = n->nextSibling();

                trixel->removeChildNode(c);
                delete c;
            }

            //Delete all pairs that represent stars
            trixel->m_nodes.clear();

            for (int c = 0; c < skyList->size(); ++c)
            {
                StarObject *star = skyList->at(c);
                if (star)
                {
                    //Add new pair with reindexed star
                    trixel->m_nodes.append(QPair<SkyObject *, SkyNode *>(star, 0));
                }
            }
        }

        if (trixelID != regionID)
        {
            trixel->hide();
            label->hide();

            if (trixel->hideCount() > delLim)
            {
                trixel->deleteAllChildNodes();
            }
        }
        else
        {
            trixel->show();
            label->show();

            if (region.hasNext())
            {
                regionID = region.next();
            }

            QLinkedList<QPair<SkyObject *, SkyNode *>> *nodes = &trixel->m_nodes;
            QLinkedList<QPair<SkyObject *, SkyNode *>>::iterator i = nodes->begin();
            bool hide = false;

            while (i != nodes->end())
            {
                bool drawLabel = false;

                StarObject *starObj = static_cast<StarObject *>((*i).first);
                SkyNode *node       = (*i).second;

                int mag = starObj->mag();

                // break loop if maglim is reached
                if (mag > maglim)
                    hide = true;
                if (!(hideLabel || mag > labelMagLim))
                    drawLabel = true;
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
                        if (!hide)
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
                    if (!hide && projector->checkVisibility(starObj))
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
        trixel = static_cast<TrixelNode *>(trixel->nextSibling());
        label  = static_cast<TrixelNode *>(label->nextSibling());

        ++trixelID;
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
    while (deep != 0)
    {
        DeepStarItem *deepStars = static_cast<DeepStarItem *>(deep);
        deep                    = deep->nextSibling();
        deepStars->update();
    }

    QSGNode *n = m_stars->firstChild();
    while (n != 0)
    {
        n = n->nextSibling();
    }
    m_skyMesh->inDraw(false);
}
