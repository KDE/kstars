/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "supernovaeitem.h"

#include "kscomet.h"
#include "labelsitem.h"
#include "Options.h"
#include "rootnode.h"
#include "satellitescomponent.h"
#include "skylabeler.h"
#include "supernovaecomponent.h"
#include "projections/projector.h"
#include "skynodes/supernovanode.h"

SupernovaeItem::SupernovaeItem(SupernovaeComponent *snovaComp, RootNode *rootNode)
    : SkyItem(LabelsItem::label_t::SATELLITE_LABEL, rootNode), m_snovaComp(snovaComp)
{
    recreateList();
}

void SupernovaeItem::update()
{
    if (!m_snovaComp->selected())
    {
        hide();
        return;
    }
    show();

    QSGNode *n = firstChild();

    while (n != 0)
    {
        SupernovaNode *sNode = static_cast<SupernovaNode *>(n);
        sNode->update();

        n = n->nextSibling();
    }
}

void SupernovaeItem::recreateList()
{
    foreach (SkyObject *so, m_snovaComp->objectList())
    {
        Supernova *sup = static_cast<Supernova*>(so);

        if (sup)
            appendChildNode(new SupernovaNode(sup));
    }
}
