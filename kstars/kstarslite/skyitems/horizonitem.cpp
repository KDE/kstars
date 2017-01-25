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
#include "skynodes/labelnode.h"

#include "rootnode.h"
#include "Options.h"

HorizonItem::HorizonItem(HorizonComponent * hComp, RootNode *rootNode)
    :SkyItem(LabelsItem::label_t::HORIZON_LABEL, rootNode), m_horizonComp(hComp)
{
    appendChildNode(new HorizonNode(m_horizonComp->pointList()));

    double az = -0.01;
    static QString name[8];
    name[0] = i18nc( "Northeast", "NE" );
    name[1] = i18nc( "East", "E" );
    name[2] = i18nc( "Southeast", "SE" );
    name[3] = i18nc( "South", "S" );
    name[4] = i18nc( "Southwest", "SW" );
    name[5] = i18nc( "West", "W" );
    name[6] = i18nc( "Northwest", "NW" );
    name[7] = i18nc( "North", "N" );

    for ( int i = 0; i < 8; i++ ) {
        SkyPoint *c = new SkyPoint;

        az += 45.0;
        c->setAz( az );
        c->setAlt( 0.0 );
        LabelNode *compass = rootNode->labelsItem()->addLabel(name[i],labelType());

        m_compassLabels.insert(c,compass);

    }
}

void HorizonItem::update() {
    if(!childCount()) {
        appendChildNode(new HorizonNode(m_horizonComp->pointList()));
    }

    //HorizonNode *hNode = static_cast<HorizonNode *>(n->skyNodeAtIndex(0));
    QSGNode *n = firstChild();
    SkyNode *hNode = static_cast<SkyNode *>(n);
    if(m_horizonComp->selected()) {
        QPointF cpoint;
        bool visible;

        const Projector *proj = SkyMapLite::Instance()->projector();
        KStarsData *data = KStarsData::Instance();

        QMap<SkyPoint *,LabelNode *>::const_iterator i = m_compassLabels.cbegin();

        while (  i != m_compassLabels.cend() ) {
            SkyPoint *c = i.key();
            if ( !Options::useAltAz() ) {
                c->HorizontalToEquatorial( data->lst(), data->geo()->lat() );
            }

            LabelNode * compass = (*i);
            cpoint = proj->toScreen( c, false, &visible );
            if ( visible && proj->onScreen(cpoint) ) {
                compass->setLabelPos(cpoint);
            } else {
                compass->hide();
            }
            i++;
        }

        hNode->update();
    } else {
        hNode->hide();
        rootNode()->labelsItem()->hideLabels(LabelsItem::label_t::HORIZON_LABEL);
    }
    n = n->nextSibling();
}

