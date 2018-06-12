/** *************************************************************************
                          supernovaeitem.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 26/06/2016
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
