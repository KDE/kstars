/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "satellitesitem.h"
#include "satellitescomponent.h"

#include "Options.h"
#include "projections/projector.h"
#include "kscomet.h"

#include "rootnode.h"
#include "labelsitem.h"
#include "satellite.h"
#include "skylabeler.h"

#include "skynodes/satellitenode.h"

SatellitesItem::SatellitesItem(SatellitesComponent *satComp, RootNode *rootNode)
    : SkyItem(LabelsItem::label_t::SATELLITE_LABEL, rootNode), m_satComp(satComp)
{
    recreateList();
    Options::setDrawSatellitesLikeStars(false);
}

void SatellitesItem::update()
{
    if (!m_satComp->selected())
    {
        hide();
        return;
    }

    QSGNode *n = firstChild();

    while (n != 0)
    {
        SatelliteNode *satNode = static_cast<SatelliteNode *>(n);
        Satellite *sat         = satNode->sat();
        if (sat->selected())
        {
            if (Options::showVisibleSatellites())
            {
                if (sat->isVisible())
                {
                    satNode->update();
                }
                else
                {
                    satNode->hide();
                }
            }
            else
            {
                satNode->update();
            }
        }
        else
        {
            satNode->hide();
        }
        n = n->nextSibling();
    }
}

void SatellitesItem::recreateList()
{
    QList<SatelliteGroup *> list = m_satComp->groups();
    for (int i = 0; i < list.size(); ++i)
    {
        SatelliteGroup *group = list.at(i);
        for (int c = 0; c < group->size(); ++c)
        {
            appendChildNode(new SatelliteNode(group->at(c), rootNode()));
        }
    }
}
