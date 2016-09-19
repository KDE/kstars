/** *************************************************************************
                          satellitesitem.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 16/05/2016
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

#include "satellitesitem.h"
#include "satellitescomponent.h"

#include "Options.h"
#include "projections/projector.h"
#include "kscomet.h"

#include "rootnode.h"
#include "labelsitem.h"
#include "skylabeler.h"

#include "skynodes/satellitenode.h"

SatellitesItem::SatellitesItem(SatellitesComponent *satComp, RootNode *rootNode)
    :SkyItem(LabelsItem::label_t::SATELLITE_LABEL, rootNode), m_satComp(satComp)
{
    recreateList();
    Options::setDrawSatellitesLikeStars(false);
}

void SatellitesItem::update() {
    if( ! m_satComp->selected() ) {
        hide();
        return;
    }

    QSGNode *n = firstChild();

    while(n != 0) {
        SatelliteNode *satNode = static_cast<SatelliteNode *>(n);
        Satellite *sat = satNode->sat();
        if ( sat->selected() ) {
            if ( Options::showVisibleSatellites() ) {
                if ( sat->isVisible() ) {
                    satNode->update();
                } else {
                    satNode->hide();
                }
            } else {
                satNode->update();
            }
        } else {
            satNode->hide();
        }
        n = n->nextSibling();
    }
}

void SatellitesItem::recreateList() {
    QList<SatelliteGroup*> list = m_satComp->groups();
    for(int i = 0; i < list.size(); ++i) {
        SatelliteGroup *group = list.at(i);
        for(int c = 0; c < group->size(); ++c) {
            appendChildNode(new SatelliteNode(group->at(c), rootNode()));
        }
    }
}
