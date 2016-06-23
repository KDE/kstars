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
    :trixel(trixelID)
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
                if(dso) {
                    DSOSymbolNode *dsoSymbol = new DSOSymbolNode(dso, indexNode->m_color);
                    symbols->appendChildNode(dsoSymbol);

                    DeepSkyNode *dsoNode = new DeepSkyNode(dso, dsoSymbol,i.key(), indexNode->m_labelType);
                    trixel->appendChildNode(dsoNode);
                }
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

    drawFlag = Options::showMessier() &&
            ! ( Options::hideOnSlew() && Options::hideMessier() && SkyMapLite::IsSlewing() );

    updateDeepSkyNode(m_Messier, drawFlag, "MessColor", &region, Options::showMessierImages() );

    drawFlag = Options::showNGC() &&
            ! ( Options::hideOnSlew() && Options::hideNGC() && SkyMapLite::IsSlewing() );

    updateDeepSkyNode(m_NGC, drawFlag, "NGCColor", &region );

    drawFlag = Options::showIC() &&
            ! ( Options::hideOnSlew() && Options::hideIC() && SkyMapLite::IsSlewing() );

    updateDeepSkyNode(m_IC, drawFlag, "ICColor", &region );

    drawFlag = Options::showOther() &&
            ! ( Options::hideOnSlew() && Options::hideOther() && SkyMapLite::IsSlewing() );

    updateDeepSkyNode(m_other, drawFlag, "NGCColor", &region );
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
    //const Projector *proj = map->projector();
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
    while( trixel != 0 ) {
        if(trixel->trixel < regionID) {
            trixel->hide();
            trixel->m_labels->hide();
            //label->hide();

            trixel = static_cast<DSOTrixelNode *>(trixel->nextSibling());
            //label = static_cast<TrixelNode *>(label->nextSibling());
            continue;
        } else if(trixel->trixel > regionID) {
            if (region->hasNext()) {
                regionID = region->next();
            } else {
                while(trixel != 0) {
                    trixel->hide();
                    trixel->m_labels->hide();

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
        }

        QSGNode *n = trixel->firstChild();

        while(n != 0 && n != trixel->m_symbols) {
            DeepSkyNode *dsoNode = static_cast<DeepSkyNode *>(n);
            n = n->nextSibling();

            DeepSkyObject *obj = static_cast<DeepSkyObject *>(dsoNode->dsObject());
            //if ( obj->drawID == drawID ) continue;  // only draw each line once
            //obj->drawID = drawID;

            if ( obj->updateID != updateID ) {
                obj->updateID = updateID;
                if ( obj->updateNumID != updateNumID) {
                    obj->updateCoords( data->updateNum() );
                }
                obj->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
            }

            float mag = obj->mag();
            float size = obj->a() * dms::PI * Options::zoomFactor() / 10800.0;

            //only draw objects if flags set, it's bigger than 1 pixel (unless
            //zoom > 2000.), and it's brighter than maglim (unless mag is
            //undefined (=99.9)
            bool sizeCriterion = (size > 1.0 || Options::zoomFactor() > 2000.);
            bool magCriterion = ( mag < (float)maglim ) || ( showUnknownMagObjects && ( std::isnan( mag ) || mag > 36.0 ) );
            if ( sizeCriterion && magCriterion )
            {
                bool drawLabel = false;
                if ( !( m_hideLabels || mag > labelMagLim ) ) drawLabel = true;

                dsoNode->update(drawImage, drawLabel);
                /*bool drawn = skyp->drawDeepSkyObject(obj, drawImage);

                //FIXME: find a better way to do above*/
            } else {
                dsoNode->hide();
            }
        }

        trixel = static_cast<DSOTrixelNode *>(trixel->nextSibling());
        //label = static_cast<TrixelNode *>(label->nextSibling());
    }
    region->reset();
}
