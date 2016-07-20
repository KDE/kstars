/** *************************************************************************
                          planetsitem.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 02/05/2016
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

#include "constellationartitem.h"
#include "projections/projector.h"
#include "constellationartcomponent.h"
#include "skynodes/constellationartnode.h"

ConstellationArtItem::ConstellationArtItem(ConstellationArtComponent *artComp, RootNode *rootNode)
    :SkyItem(LabelsItem::label_t::NO_LABEL, rootNode), m_artComp(artComp)
{
    QList<ConstellationsArt *>list = m_artComp->m_ConstList;
    for(int i = 0; i < list.size(); ++i) {
        ConstellationArtNode *constArt = new ConstellationArtNode(list.at(i));
        appendChildNode(constArt);
    }
}

void ConstellationArtItem::update() {
    if(Options::showConstellationArt()) {
        m_artComp->loadData();
        if (SkyMapLite::IsSlewing() == false) {
            show();
            //Traverse all children nodes of RootNode
            QSGNode *n = firstChild();
            while(n != 0) {
                ConstellationArtNode *artNode = static_cast<ConstellationArtNode *>(n);
                artNode->update();
                n = n->nextSibling();
            }
        } else {
            hide();
        }
    } else {
        //Delete all images if we don't need to draw constellation art and save ~50 MB.
        m_artComp->deleteData();
        QSGNode *n = firstChild();
        while(n != 0) {
            QSGNode *d = n;
            n = n->nextSibling();
            removeChildNode(d);
            delete d;
        }
    }
}
