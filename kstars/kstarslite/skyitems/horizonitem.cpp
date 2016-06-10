/** *************************************************************************
                          horizonitem.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 28/05/2016
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

#include "horizonitem.h"

#include "horizoncomponent.h"
#include "skynodes/horizonnode.h"
#include "labelsitem.h"

HorizonItem::HorizonItem(HorizonComponent * hComp, RootNode *rootNode)
    :SkyItem(LabelsItem::label_t::RUDE_LABEL, rootNode), m_horizonComp(hComp)
{
    appendChildNode(new HorizonNode(m_horizonComp->pointList()));
}


void HorizonItem::update() {
    if(!childCount()) {
        appendChildNode(new HorizonNode(m_horizonComp->pointList()));
    }

    //HorizonNode *hNode = static_cast<HorizonNode *>(n->skyNodeAtIndex(0));
    QSGNode *n = firstChild();
    while(n != 0) {
        SkyNode *hNode = static_cast<SkyNode *>(n);
        if(m_horizonComp->selected()) {
            hNode->update();
        } else {
            hNode->hide();
        }
        n = n->nextSibling();
    }
}

