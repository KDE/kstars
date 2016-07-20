/** *************************************************************************
                          deepskyitem.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 18/06/2016
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

#include "skynodes/deepskynode.h"
#include "labelsitem.h"
#include "deepskyitem.h"
#include "deepstaritem.h"
#include "skynodes/dsosymbolnode.h"

#include "starcomponent.h"
#include "starblockfactory.h"
#include "skymesh.h"
#include "rootnode.h"

DSOIndexNode::DSOIndexNode(DeepSkyIndex *index, LabelsItem::label_t labelType, QString colorString)
    :m_index(index), m_trixels(new QSGNode), m_labelType(labelType)
{
    m_color = KStarsData::Instance()->colorScheme()->colorNamed( colorString );
    appendChildNode(m_trixels);
}

void DSOIndexNode::hide() {
    SkyOpacityNode::hide();
    SkyMapLite::Instance()->rootNode()->labelsItem()->hideLabels(m_labelType);
}

void DSOIndexNode::show() {
    SkyOpacityNode::show();
    SkyMapLite::Instance()->rootNode()->labelsItem()->showLabels(m_labelType);
}

DSOTrixelNode::DSOTrixelNode(Trixel trixelID)
    :TrixelNode(trixelID)
{

}

DeepSkyItem::DeepSkyItem(DeepSkyComponent *dsoComp, RootNode *rootNode)
    :SkyItem(LabelsItem::label_t::DEEP_SKY_LABEL, rootNode), m_dsoComp(dsoComp),
      m_skyMesh(SkyMesh::Instance())
{
    m_Messier = new DSOIndexNode(&(m_dsoComp->m_MessierIndex), LabelsItem::label_t::DSO_MESSIER_LABEL,
                                 "MessColor");
    appendChildNode(m_Messier);

    m_NGC = new DSOIndexNode(&(m_dsoComp->m_NGCIndex), LabelsItem::label_t::DSO_NGC_LABEL, "NGCColor");
    appendChildNode(m_NGC);

    m_IC = new DSOIndexNode(&(m_dsoComp->m_ICIndex), LabelsItem::label_t::DSO_IC_LABEL, "ICColor");
    appendChildNode(m_IC);

    m_other = new DSOIndexNode(&(m_dsoComp->m_OtherIndex), LabelsItem::label_t::DSO_OTHER_LABEL,
                               "NGCColor");
    appendChildNode(m_other);

    QSGNode *n = firstChild();

    while(n != 0 ) {
        DSOIndexNode *indexNode = static_cast<DSOIndexNode *>(n);
        DeepSkyIndex *index = indexNode->m_index;

        QMap<int, DeepSkyList* > result;
        QHashIterator<int, DeepSkyList*> it(*index);

        while (it.hasNext()) {
            it.next();
            result.insert(it.key(), it.value());
        }

        QMap<int, DeepSkyList* >::const_iterator i = result.constBegin();

        while(i != result.constEnd()) {
            DeepSkyList *dsoList = i.value();

            DSOTrixelNode *trixel = new DSOTrixelNode(i.key());
            trixel->m_labels = rootNode->labelsItem()->addTrixel(indexNode->m_labelType, i.key());

            indexNode->m_trixels->appendChildNode(trixel);
            QSGNode *symbols = new QSGNode;

            for(int c = 0; c < dsoList->size(); ++c) {

                DeepSkyObject *dso = dsoList->at(c);
                trixel->m_nodes.append(QPair<SkyObject *, SkyNode *>(dso, 0));
            }

            trixel->m_symbols = symbols;
            trixel->appendChildNode(symbols);

            ++i;
        }
        n = n->nextSibling();
    }
}

void DeepSkyItem::update() {
    if( !m_dsoComp->selected() ) {
        hide();
        return;
    }

    bool drawFlag;

    MeshIterator region( m_skyMesh, DRAW_BUF );

    int count = 0;

    drawFlag = Options::showMessier() &&
            ! ( Options::hideOnSlew() && Options::hideMessier() && SkyMapLite::IsSlewing() );

    updateDeepSkyNode(m_Messier, drawFlag, "MessColor", &region, Options::showMessierImages() );

    /*QSGNode *n = m_Messier->m_trixels->firstChild();
    while(n != 0) {
        count += n->childCount();
        n = n->nextSibling();
    }*/

    drawFlag = Options::showNGC() &&
            ! ( Options::hideOnSlew() && Options::hideNGC() && SkyMapLite::IsSlewing() );

    updateDeepSkyNode(m_NGC, drawFlag, "NGCColor", &region );

    /*n = m_NGC->m_trixels->firstChild();
    while(n != 0) {
        count += n->childCount();
        n = n->nextSibling();
    }*/

    drawFlag = Options::showIC() &&
            ! ( Options::hideOnSlew() && Options::hideIC() && SkyMapLite::IsSlewing() );

    updateDeepSkyNode(m_IC, drawFlag, "ICColor", &region );

    /*n = m_IC->m_trixels->firstChild();
    while(n != 0) {
        count += n->childCount();
        n = n->nextSibling();
    }*/

    drawFlag = Options::showOther() &&
            ! ( Options::hideOnSlew() && Options::hideOther() && SkyMapLite::IsSlewing() );

    updateDeepSkyNode(m_other, drawFlag, "NGCColor", &region );

    /*n = m_other->m_trixels->firstChild();
    while(n != 0) {
        count += n->childCount();
        n = n->nextSibling();
    }

    qDebug() << count << "DSO";*/
}

void DeepSkyItem::updateDeepSkyNode(DSOIndexNode *indexNode, bool drawObject, const QString& colorString,
                                    MeshIterator *region, bool drawImage)
{
    if ( ! ( drawObject || drawImage ) ) {
        indexNode->hide();
        return;
    }

    indexNode->show();

    SkyMapLite *map = SkyMapLite::Instance();
    const Projector *projector = map->projector();
    KStarsData *data = KStarsData::Instance();

    UpdateID updateID = data->updateID();
    UpdateID updateNumID = data->updateNumID();

    //WARNING: CHECK COLOR
    /*skyp->setPen( data->colorScheme()->colorNamed( colorString ) );
    skyp->setBrush( Qt::NoBrush );*/

    bool m_hideLabels =  ( map->isSlewing() && Options::hideOnSlew() ) ||
            ! ( Options::showDeepSkyMagnitudes() || Options::showDeepSkyNames() );


    double maglim = Options::magLimitDrawDeepSky();
    bool showUnknownMagObjects = Options::showUnknownMagObjects();

    //adjust maglimit for ZoomLevel
    double lgmin = log10(MINZOOM);
    double lgmax = log10(MAXZOOM);
    double lgz = log10(Options::zoomFactor());
    if ( lgz <= 0.75 * lgmax )
        maglim -= (Options::magLimitDrawDeepSky() - Options::magLimitDrawDeepSkyZoomOut() )*(0.75*lgmax - lgz)/(0.75*lgmax - lgmin);
    double m_zoomMagLimit = maglim;

    double labelMagLim = Options::deepSkyLabelDensity();
    labelMagLim += ( Options::magLimitDrawDeepSky() - labelMagLim ) * ( lgz - lgmin) / (lgmax - lgmin );
    if ( labelMagLim > Options::magLimitDrawDeepSky() ) labelMagLim = Options::magLimitDrawDeepSky();

    //DrawID drawID = m_skyMesh->drawID();

    int regionID = -1;
    if(region->hasNext()) {
        regionID = region->next();
    }

    /*//Check that at least one trixel is shown

    if(regionID == -1) {
        hide();
        return;
    } else {
        show();
    }*/

    DSOTrixelNode *trixel = static_cast<DSOTrixelNode *>(indexNode->m_trixels->firstChild());
    double delLim = SkyMapLite::deleteLimit();

    while( trixel != 0 ) {
        if(trixel->trixelID() < regionID) {
            trixel->hide();
            trixel->m_labels->hide();

            if(trixel->hideCount() > delLim) {
                QLinkedList<QPair<SkyObject *, SkyNode *>>::iterator i = trixel->m_nodes.begin();

                while(i != trixel->m_nodes.end()) {
                    DeepSkyNode *node = static_cast<DeepSkyNode *>((*i).second);
                    if(node) {
                        node->parent()->removeChildNode(node);
                        node->destroy();

                        *i = QPair<SkyObject *, SkyNode *>((*i).first, 0);
                    }
                    i++;
                }
            }
        } else if(trixel->trixelID() > regionID) {
            if (region->hasNext()) {
                regionID = region->next();
            } else {
                while(trixel != 0) {
                    trixel->hide();
                    trixel->m_labels->hide();

                    if(trixel->hideCount() > delLim) {
                        QLinkedList<QPair<SkyObject *, SkyNode *>>::iterator i = trixel->m_nodes.begin();

                        while(i != trixel->m_nodes.end()) {
                            DeepSkyNode *node = static_cast<DeepSkyNode *>((*i).second);
                            if(node) {
                                node->parent()->removeChildNode(node);
                                node->destroy();

                                *i = QPair<SkyObject *, SkyNode *>((*i).first, 0);
                            }
                            i++;
                        }
                    }

                    trixel = static_cast<DSOTrixelNode *>(trixel->nextSibling());
                }
                break;
            }
            continue;
        } else {
            trixel->show();
            trixel->m_labels->show();

            if(region->hasNext()) {
                regionID = region->next();
            }

            QLinkedList<QPair<SkyObject *, SkyNode *>> *nodes = &trixel->m_nodes;

            QLinkedList<QPair<SkyObject *, SkyNode *>>::iterator i = nodes->begin();

            while(i != nodes->end()) {
                DeepSkyObject *dsoObj = static_cast<DeepSkyObject *>((*i).first);
                DeepSkyNode *dsoNode = static_cast<DeepSkyNode *>((*i).second);

                if ( dsoObj->updateID != updateID ) {
                    dsoObj->updateID = updateID;
                    if ( dsoObj->updateNumID != updateNumID) {
                        dsoObj->updateCoords( data->updateNum() );
                    }
                    dsoObj->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
                }

                float mag = dsoObj->mag();
                float size = dsoObj->a() * dms::PI * Options::zoomFactor() / 10800.0;

                //only draw objects if flags set, it's bigger than 1 pixel (unless
                //zoom > 2000.), and it's brighter than maglim (unless mag is
                //undefined (=99.9)
                bool sizeCriterion = (size > 1.0 || Options::zoomFactor() > 2000.);
                bool magCriterion = ( mag < (float)maglim ) || ( showUnknownMagObjects && ( std::isnan( mag ) || mag > 36.0 ) );

                bool drawLabel = false;

                if ( !( m_hideLabels || mag > labelMagLim ) ) drawLabel = true;

                if( dsoNode ) {
                    if( dsoNode->hideCount() > delLim ) {
                        //qDebug() << dsoNode->hideCount() << "hideCount";
                        trixel->removeChildNode(dsoNode);
                        dsoNode->destroy();
                        *i = QPair<SkyObject *, SkyNode *>((*i).first, 0);
                    } else {
                        if(sizeCriterion && magCriterion) {

                            dsoNode->update(drawImage, drawLabel);
                        } else {
                            dsoNode->hide();
                        }
                    }
                } else {
                    if( sizeCriterion && magCriterion && projector->checkVisibility(dsoObj) ) {

                        QPointF pos;

                        bool visible = false;
                        pos = projector->toScreen(dsoObj,true,&visible);
                        if( visible && projector->onScreen(pos) ) {
                            DSOSymbolNode *dsoSymbol = new DSOSymbolNode(dsoObj, indexNode->m_color);
                            trixel->m_symbols->appendChildNode(dsoSymbol);

                            DeepSkyNode *dsoNode = new DeepSkyNode(dsoObj, dsoSymbol, trixel->trixelID(), indexNode->m_labelType);
                            trixel->appendChildNode(dsoNode);

                            *i = QPair<SkyObject *, SkyNode *>((*i).first, static_cast<SkyNode *>(dsoNode));
                            dsoNode->update(drawImage, drawLabel, pos);
                        }
                    }
                }
                i++;
            }
        }
        trixel = static_cast<DSOTrixelNode *>(trixel->nextSibling());
    }

    region->reset();
}
