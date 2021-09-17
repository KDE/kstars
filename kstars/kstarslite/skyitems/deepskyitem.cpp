/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "deepskyitem.h"

#include "deepskyobject.h"
#include "deepstaritem.h"
#include "labelsitem.h"
#include "Options.h"
#include "rootnode.h"
#include "skymesh.h"
#include "starblockfactory.h"
#include "starcomponent.h"
#include "htmesh/MeshIterator.h"
#include "projections/projector.h"
#include "skynodes/deepskynode.h"
#include "skynodes/dsosymbolnode.h"

DSOIndexNode::DSOIndexNode(DeepSkyIndex *index, LabelsItem::label_t labelType, QString color)
    : m_index(index), m_trixels(new QSGNode), m_labelType(labelType), schemeColor(color)
{
    appendChildNode(m_trixels);
}

void DSOIndexNode::hide()
{
    SkyOpacityNode::hide();
    SkyMapLite::Instance()->rootNode()->labelsItem()->hideLabels(m_labelType);
}

void DSOIndexNode::show()
{
    SkyOpacityNode::show();
    SkyMapLite::Instance()->rootNode()->labelsItem()->showLabels(m_labelType);
}

DSOTrixelNode::DSOTrixelNode(Trixel trixelID) : TrixelNode(trixelID)
{
}

void DSOTrixelNode::deleteAllChildNodes()
{
    QLinkedList<QPair<SkyObject *, SkyNode *>>::iterator i = m_nodes.begin();

    while (i != m_nodes.end())
    {
        DeepSkyNode *node = static_cast<DeepSkyNode *>((*i).second);
        if (node)
        {
            node->parent()->removeChildNode(node);
            delete node->symbol();
            delete node;

            *i = QPair<SkyObject *, SkyNode *>((*i).first, 0);
        }
        ++i;
    }
}

DeepSkyItem::DeepSkyItem(DeepSkyComponent *dsoComp, RootNode *rootNode)
    : SkyItem(LabelsItem::label_t::DEEP_SKY_LABEL, rootNode), m_dsoComp(dsoComp), m_skyMesh(SkyMesh::Instance())
{

    m_other = new DSOIndexNode(&(m_dsoComp->m_OtherIndex), LabelsItem::label_t::DSO_OTHER_LABEL, "NGCColor");
    appendChildNode(m_other);

    QSGNode *n = firstChild();

    while (n != 0)
    {
        DSOIndexNode *indexNode = static_cast<DSOIndexNode *>(n);
        DeepSkyIndex *index     = indexNode->m_index;

        QMap<int, DeepSkyList *> result;
        QHashIterator<int, DeepSkyList *> it(*index);

        while (it.hasNext())
        {
            it.next();
            result.insert(it.key(), it.value());
        }

        QMap<int, DeepSkyList *>::const_iterator i = result.constBegin();

        while (i != result.constEnd())
        {
            DeepSkyList *dsoList = i.value();

            DSOTrixelNode *trixel = new DSOTrixelNode(i.key());
            trixel->m_labels      = rootNode->labelsItem()->addTrixel(indexNode->m_labelType, i.key());

            indexNode->m_trixels->appendChildNode(trixel);
            QSGNode *symbols = new QSGNode;

            for (int c = 0; c < dsoList->size(); ++c)
            {
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

void DeepSkyItem::update()
{
    if (!m_dsoComp->selected())
    {
        hide();
        return;
    }
    show();

    bool drawFlag;

    MeshIterator region(m_skyMesh, DRAW_BUF);

    drawFlag = Options::showMessier() && !(Options::hideOnSlew() && Options::hideMessier() && SkyMapLite::IsSlewing());

    updateDeepSkyNode(m_other, drawFlag, &region);
}

void DeepSkyItem::updateDeepSkyNode(DSOIndexNode *indexNode, bool drawObject, MeshIterator *region, bool drawImage)
{
    if (!(drawObject || drawImage))
    {
        indexNode->hide();
        return;
    }

    indexNode->show();

    SkyMapLite *map            = SkyMapLite::Instance();
    const Projector *projector = map->projector();
    KStarsData *data           = KStarsData::Instance();

    UpdateID updateID    = data->updateID();
    UpdateID updateNumID = data->updateNumID();

    QColor schemeColor = data->colorScheme()->colorNamed(indexNode->schemeColor);

    bool m_hideLabels = (map->isSlewing() && Options::hideOnSlew()) ||
                        !(Options::showDeepSkyMagnitudes() || Options::showDeepSkyNames());

    double maglim              = Options::magLimitDrawDeepSky();
    bool showUnknownMagObjects = Options::showUnknownMagObjects();

    //adjust maglimit for ZoomLevel
    double lgmin = log10(MINZOOM);
    double lgmax = log10(MAXZOOM);
    double lgz   = log10(Options::zoomFactor());
    if (lgz <= 0.75 * lgmax)
        maglim -= (Options::magLimitDrawDeepSky() - Options::magLimitDrawDeepSkyZoomOut()) * (0.75 * lgmax - lgz) /
                  (0.75 * lgmax - lgmin);

    double labelMagLim = Options::deepSkyLabelDensity();
    labelMagLim += (Options::magLimitDrawDeepSky() - labelMagLim) * (lgz - lgmin) / (lgmax - lgmin);
    if (labelMagLim > Options::magLimitDrawDeepSky())
        labelMagLim = Options::magLimitDrawDeepSky();

    int regionID = 0;
    if (region->hasNext())
    {
        regionID = region->next();
    }

    DSOTrixelNode *trixel = static_cast<DSOTrixelNode *>(indexNode->m_trixels->firstChild());
    double delLim         = SkyMapLite::deleteLimit();

    /*Unlike DeepStarItem and StarItem where all trixels are set, in DeepSkyItem number of trixels is different
    across the catalogs. We are comparing trixelID and regionID to hide trixels that are not visible and skip
    regionIDs that are not present in this catalog*/
    while (trixel != 0)
    {
        //Hide all trixels that has smaller trixelID than the next visible trixel
        if (trixel->trixelID() < regionID)
        {
            trixel->hide();
            trixel->m_labels->hide();

            if (trixel->hideCount() > delLim)
            {
                trixel->deleteAllChildNodes();
            }
        }
        else if (trixel->trixelID() > regionID)
        {
            /*Keep iterating over regionID if current trixel's trixelID is larger than regionID. If there are no more
            visible regions then hide/delete remaining trixels*/
            if (region->hasNext())
            {
                regionID = region->next();
            }
            else
            {
                while (trixel != 0)
                {
                    trixel->hide();
                    trixel->m_labels->hide();

                    if (trixel->hideCount() > delLim)
                    {
                        trixel->deleteAllChildNodes();
                    }
                    trixel = static_cast<DSOTrixelNode *>(trixel->nextSibling());
                }
                break;
            }
            continue;
        }
        else //Current trixelID is equal to regionID meaning that current trixel is visible
        {
            trixel->show();
            trixel->m_labels->show();

            if (region->hasNext())
            {
                regionID = region->next();
            }

            QLinkedList<QPair<SkyObject *, SkyNode *>>::iterator i = (&trixel->m_nodes)->begin();

            while (i != (&trixel->m_nodes)->end())
            {
                DeepSkyObject *dsoObj = static_cast<DeepSkyObject *>((*i).first);
                DeepSkyNode *dsoNode  = static_cast<DeepSkyNode *>((*i).second);

                if (dsoObj->updateID != updateID)
                {
                    dsoObj->updateID = updateID;
                    if (dsoObj->updateNumID != updateNumID)
                    {
                        dsoObj->updateCoords(data->updateNum());
                    }
                    dsoObj->EquatorialToHorizontal(data->lst(), data->geo()->lat());
                }

                float mag  = dsoObj->mag();
                float size = dsoObj->a() * dms::PI * Options::zoomFactor() / 10800.0;

                //only draw objects if flags set, it's bigger than 1 pixel (unless
                //zoom > 2000.), and it's brighter than maglim (unless mag is
                //undefined (=99.9)
                bool sizeCriterion = (size > 1.0 || Options::zoomFactor() > 2000.);
                bool magCriterion = (mag < (float)maglim) || (showUnknownMagObjects && (std::isnan(mag) || mag > 36.0));

                bool drawLabel = false;

                if (!(m_hideLabels || mag > labelMagLim))
                    drawLabel = true;

                if (dsoNode)
                {
                    //Delete particular DSONode if its hideCount is larger than the limit
                    if (dsoNode->hideCount() > delLim)
                    {
                        trixel->removeChildNode(dsoNode);

                        delete dsoNode->symbol();
                        delete dsoNode;

                        *i = QPair<SkyObject *, SkyNode *>((*i).first, 0);
                    }
                    else
                    {
                        if (sizeCriterion && magCriterion)
                        {
                            dsoNode->setColor(schemeColor, trixel);
                            dsoNode->update(drawImage, drawLabel);
                        }
                        else
                        {
                            dsoNode->hide();
                        }
                    }
                }
                else
                {
                    if (sizeCriterion && magCriterion && projector->checkVisibility(dsoObj))
                    {
                        QPointF pos;

                        bool visible = false;
                        pos          = projector->toScreen(dsoObj, true, &visible);
                        if (visible && projector->onScreen(pos))
                        {
                            //Create new DeepSkyNode and its symbol if it is currently visible
                            DSOSymbolNode *dsoSymbol = new DSOSymbolNode(dsoObj, schemeColor);
                            trixel->m_symbols->appendChildNode(dsoSymbol);

                            DeepSkyNode *dsoNode =
                                new DeepSkyNode(dsoObj, dsoSymbol, indexNode->m_labelType, trixel->trixelID());
                            trixel->appendChildNode(dsoNode);

                            *i = QPair<SkyObject *, SkyNode *>((*i).first, static_cast<SkyNode *>(dsoNode));
                            dsoNode->update(drawImage, drawLabel, pos);
                        }
                    }
                }
                ++i;
            }
        }
        trixel = static_cast<DSOTrixelNode *>(trixel->nextSibling());
    }

    region->reset();
}
