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

DSOIndexNode::DSOIndexNode(DeepSkyIndex *index)
    :m_index(index), m_trixImage(new QSGNode), m_trixSymbol(new QSGNode)
{
    appendChildNode(m_trixImage);
    appendChildNode(m_trixSymbol);
}

DSOTrixelNode::DSOTrixelNode(Trixel trixelID)
    :trixel(trixelID)
{

}

DeepSkyItem::DeepSkyItem(DeepSkyComponent *dsoComp, RootNode *rootNode)
    :SkyItem(LabelsItem::label_t::DEEP_SKY_LABEL, rootNode), m_dsoComp(dsoComp),
      m_skyMesh(SkyMesh::Instance())

    /*,m_starLabels(rootNode->labelsItem()->getLabelNode(labelType())), m_stars(new SkyOpacityNode),
                                              m_deepStars(new SkyOpacityNode)*/
{
    m_Messier = new DSOIndexNode(&(m_dsoComp->m_MessierIndex));
    appendChildNode(m_Messier);

    m_NGC = new DSOIndexNode(&(m_dsoComp->m_NGCIndex));
    appendChildNode(m_NGC);

    m_IC = new DSOIndexNode(&(m_dsoComp->m_ICIndex));
    appendChildNode(m_IC);

    m_other = new DSOIndexNode(&(m_dsoComp->m_OtherIndex));
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

            DSOTrixelNode *trixelImage = new DSOTrixelNode(i.key());
            DSOTrixelNode *trixelSymbol = new DSOTrixelNode(i.key());

            indexNode->m_trixImage->appendChildNode(trixelImage);
            indexNode->m_trixSymbol->appendChildNode(trixelSymbol);

            for(int c = 0; c < dsoList->size(); ++c) {
                DeepSkyObject *dso = dsoList->at(c);
                if(dso) {
                    DSOSymbolNode *dsoSymbol = new DSOSymbolNode(dso);
                    trixelSymbol->appendChildNode(dsoSymbol);

                    DeepSkyNode *dsoNode = new DeepSkyNode(dso, dsoSymbol);
                    trixelImage->appendChildNode(dsoNode);
                }
            }
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
#ifndef KSTARS_LITE
    //adjust maglimit for ZoomLevel
    double lgmin = log10(MINZOOM);
    double lgmax = log10(MAXZOOM);
    double lgz = log10(Options::zoomFactor());
    if ( lgz <= 0.75 * lgmax )
        maglim -= (Options::magLimitDrawDeepSky() - Options::magLimitDrawDeepSkyZoomOut() )*(0.75*lgmax - lgz)/(0.75*lgmax - lgmin);
    m_zoomMagLimit = maglim;

    double labelMagLim = Options::deepSkyLabelDensity();
    labelMagLim += ( Options::magLimitDrawDeepSky() - labelMagLim ) * ( lgz - lgmin) / (lgmax - lgmin );
    if ( labelMagLim > Options::magLimitDrawDeepSky() ) labelMagLim = Options::magLimitDrawDeepSky();

    ////
    //DrawID drawID = m_skyMesh->drawID();
    MeshIterator region( m_skyMesh, DRAW_BUF );

    while ( region.hasNext() ) {

        Trixel trixel = region.next();
        DeepSkyList* dsList = dsIndex->value( trixel );
        if ( dsList == 0 ) continue;
        for (int j = 0; j < dsList->size(); j++ ) {
            DeepSkyObject *obj = dsList->at( j );

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

                bool drawn = skyp->drawDeepSkyObject(obj, drawImage);
                if ( drawn  && !( m_hideLabels || mag > labelMagLim ) )
                    addLabel( proj->toScreen(obj), obj );
                //FIXME: find a better way to do above
            }
        }
    }
#endif
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

    DSOTrixelNode *trixelImg = static_cast<DSOTrixelNode *>(indexNode->m_trixImage->firstChild());
    DSOTrixelNode *trixelSymbol = static_cast<DSOTrixelNode *>(indexNode->m_trixSymbol->firstChild());
    while( trixelImg != 0 ) {
        if(trixelImg->trixel < regionID) {
            trixelImg->hide();
            trixelSymbol->hide();
            //label->hide();

            trixelImg = static_cast<DSOTrixelNode *>(trixelImg->nextSibling());
            trixelSymbol = static_cast<DSOTrixelNode *>(trixelSymbol->nextSibling());
            //label = static_cast<TrixelNode *>(label->nextSibling());
            continue;
        } else if(trixelImg->trixel > regionID) {
            if (region->hasNext()) {
                regionID = region->next();
            } else {
                while(trixelImg != 0) {
                    trixelImg->hide();
                    trixelSymbol->hide();

                    trixelImg = static_cast<DSOTrixelNode *>(trixelImg->nextSibling());
                    trixelSymbol = static_cast<DSOTrixelNode *>(trixelSymbol->nextSibling());
                }
                break;
            }
            continue;
        } else {
            trixelImg->show();
            trixelSymbol->show();
            //label->show();
            if(region->hasNext()) {
                regionID = region->next();
            }
        }

        QSGNode *n = trixelImg->firstChild();

        while(n != 0) {
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
                dsoNode->update(drawImage);
                /*bool drawn = skyp->drawDeepSkyObject(obj, drawImage);
                if ( drawn  && !( m_hideLabels || mag > labelMagLim ) )
                    addLabel( proj->toScreen(obj), obj );
                //FIXME: find a better way to do above*/
            } else {
                dsoNode->hide();
            }
        }

        trixelImg = static_cast<DSOTrixelNode *>(trixelImg->nextSibling());
        trixelSymbol = static_cast<DSOTrixelNode *>(trixelSymbol->nextSibling());
        //label = static_cast<TrixelNode *>(label->nextSibling());
    }
    region->reset();
}
