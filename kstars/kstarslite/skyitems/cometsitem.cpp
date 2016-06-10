/** *************************************************************************
                          cometsitem.cpp  -  K Desktop Planetarium
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

#include "cometsitem.h"

#include "Options.h"
#include "projections/projector.h"
#include "kscomet.h"

#include "rootnode.h"
#include "labelsitem.h"
#include "skylabeler.h"

#include "skynodes/pointsourcenode.h"

CometsItem::CometsItem(const QList<SkyObject*>& cometsList, RootNode *rootNode)
    :SkyItem(LabelsItem::label_t::COMET_LABEL, rootNode), m_cometsList(cometsList)
{
    recreateList();
}

void CometsItem::update() {
    if(Options::zoomFactor() < 10*MINZOOM) {
        hide();
        return;
    }

    show();

    bool hideLabels =  ! Options::showCometNames() ||
                       (SkyMapLite::Instance()->isSlewing() &&
                        Options::hideLabels() );
    double rsunLabelLimit = Options::maxRadCometName();

    /*//FIXME: Should these be config'able?
    skyp->setPen( QPen( QColor( "darkcyan" ) ) );
    skyp->setBrush( QBrush( QColor( "darkcyan" ) ) );*/

    //Traverse all children nodes
    QSGNode *n = firstChild();
    while( n != 0) {
        SkyNode* skyNode = static_cast<SkyNode*>(n);
        n = n->nextSibling();

        //TODO: Might be better move it to PointSourceNode
        KSComet *com = static_cast<KSComet *>(skyNode->skyObject());
        double mag = com->mag();
        if (std::isnan(mag) == 0)
        {
            bool drawLabel = false;
            if ( !(hideLabels || com->rsun() >= rsunLabelLimit) ) {
                drawLabel = true;
            }
            skyNode->update(drawLabel);
        } else {
            skyNode->hide();
        }
    }
}

void CometsItem::recreateList() {
    removeAllChildNodes();
    foreach(SkyObject *comet, m_cometsList) {
        KSComet *com = static_cast<KSComet *>(comet);
        appendChildNode(new PointSourceNode(com, rootNode(),labelType()));
    }
}
