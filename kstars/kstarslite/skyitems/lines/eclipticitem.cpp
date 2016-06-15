/** *************************************************************************
                          eclipticitem.cpp  -  K Desktop Planetarium
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

#include "eclipticitem.h"

#include "../rootnode.h"
#include "../labelsitem.h"
#include "ecliptic.h"
#include "../skynodes/labelnode.h"
#include "../skynodes/trixelnode.h"

EclipticItem::EclipticItem(Ecliptic *eclipticComp, RootNode *rootNode)
    :SkyItem(LabelsItem::label_t::ECLIPTIC_LABEL, rootNode), m_eclipticComp(eclipticComp)
{
    LineListHash *trixels = eclipticComp->lineIndex();

    QHash< Trixel, LineListList *>::const_iterator i = trixels->begin();
    while( i != trixels->end()) {

        TrixelNode *trixel = new TrixelNode(i.key(), i.value(), "EclColor", 1, Qt::SolidLine );
        appendChildNode(trixel);
        ++i;
    }

    KStarsData *data = KStarsData::Instance();

    KSNumbers num( data->ut().djd() );
    dms elat(0.0), elng(0.0);
    QString label;
    for( int ra = 0; ra < 23; ra += 6 ) {
        elng.setH( ra );
        SkyPoint *o = new SkyPoint;
        o->setFromEcliptic( num.obliquity(), elng, elat );
        o->setRA0(  o->ra()  );
        o->setDec0( o->dec() );
        o->EquatorialToHorizontal( data->lst(), data->geo()->lat() );

        label.setNum( o->ra().reduce().Degrees() );

        LabelNode *compass = rootNode->labelsItem()->addLabel(label, labelType());
        m_compassLabels.insert(o, compass);
    }
}

void EclipticItem::update() {
    if(m_eclipticComp->selected()) {
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

