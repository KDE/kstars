/** *************************************************************************
                          telescopesymbolsitem.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 17/07/2016
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

#include "skynodes/crosshairnode.h"
#include "indi/clientmanagerlite.h"
#include "telescopesymbolsitem.h"
#include "Options.h"
#include "projections/projector.h"
#include "kstarslite/skyitems/rootnode.h"
#include "kstarslite.h"

#include "labelsitem.h"

TelescopeSymbolsItem::TelescopeSymbolsItem(RootNode *rootNode)
    :SkyItem(LabelsItem::label_t::TELESCOPE_SYMBOL, rootNode)
{
    m_clientManager = KStarsLite::Instance()->clientManagerLite();
    m_KStarsData = KStarsData::Instance();
}

void TelescopeSymbolsItem::addTelescope(INDI::BaseDevice *bd) {
    if(!m_telescopes.value(bd)) {
        CrosshairNode *crossHair = new CrosshairNode(bd, rootNode());
        appendChildNode(crossHair);

        m_telescopes.insert(bd, crossHair);
    }
}

void TelescopeSymbolsItem::removeTelescope(INDI::BaseDevice *bd) {
    CrosshairNode *crossHair = m_telescopes.value(bd);
    if(crossHair) {
        removeChildNode(crossHair);
        delete crossHair;
    }
    m_telescopes.remove(bd);
}

void TelescopeSymbolsItem::update() {
    QHash<INDI::BaseDevice *, CrosshairNode *>::iterator i;
    bool deleteAll = !m_clientManager->isConnected();

    QColor color = m_KStarsData->colorScheme()->colorNamed("TargetColor" );

    bool show = Options::showTargetCrosshair();
    if(!show) {
        hide();
    }

    for (i = m_telescopes.begin(); i != m_telescopes.end(); ++i) {
        CrosshairNode *crossHair = i.value();
        INDI::BaseDevice *device = i.key();
        if(crossHair) {
            if(deleteAll || !(device->isConnected())) {
                removeChildNode(crossHair);
                delete crossHair;
                m_telescopes.insert(device, nullptr);
            } else if(show) {
                if(device->isConnected()){
                    crossHair->setColor(color);
                    crossHair->update();
                } else {
                    crossHair->hide();
                }
            }
        }
    }
    if(deleteAll) {
        m_telescopes.clear();
    }
}
