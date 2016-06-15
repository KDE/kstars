/** *************************************************************************
                          equatoritem.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 10/06/2016
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

#include "Options.h"
#include "projections/projector.h"
#include <QSGNode>

#include "equatoritem.h"

#include "../skynodes/trixelnode.h"
#include "../rootnode.h"
#include "../labelsitem.h"

#include "../skynodes/labelnode.h"

EquatorItem::EquatorItem(Equator *equatorComp, RootNode *rootNode)
    :SkyItem(LabelsItem::label_t::EQUATOR_LABEL, rootNode), m_equatorComp(equatorComp)
{
    LineListHash *trixels = equatorComp->lineIndex();

    QHash< Trixel, LineListList *>::const_iterator i = trixels->begin();
    while( i != trixels->end()) {

        TrixelNode *trixel = new TrixelNode(i.key(), i.value(), "EqColor", 1, Qt::SolidLine );
        appendChildNode(trixel);
        ++i;
    }

    for( int ra = 0; ra < 23; ra += 2 ) {
        SkyPoint *o = new SkyPoint( ra, 0.0 );

        QString label;
        label.setNum( o->ra().hour() );

        LabelNode *compass = rootNode->labelsItem()->addLabel(label, labelType());
        m_compassLabels.insert(o, compass);
    }
}

void EquatorItem::update() {
    if(m_equatorComp->selected()) {
        show();
        QSGNode *n = firstChild();
        while(n != 0) {
            TrixelNode * trixel = static_cast<TrixelNode *>(n);
            n = n->nextSibling();
            trixel->update();
        }

        const Projector *proj  = SkyMapLite::Instance()->projector();
        KStarsData *data       = KStarsData::Instance();

        QMap<SkyPoint *,LabelNode *>::iterator i = m_compassLabels.begin();

        while (  i != m_compassLabels.end() ) {
            SkyPoint *c = i.key();
            c->EquatorialToHorizontal( data->lst(), data->geo()->lat() );

            LabelNode * compass = (*i);

            bool visible;
            QPointF cpoint = proj->toScreen( c, false, &visible );
            if ( visible && proj->checkVisibility( c ) ) {
                compass->setLabelPos(cpoint);
            } else {
                compass->hide();
            }
            i++;
        }

    } else {
        hide();
    }
}
