/** *************************************************************************
                          milkywayitem.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 23/06/2016
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

#include "milkywayitem.h"
#include "milkyway.h"
#include "linelist.h"
#include "linelistindex.h"
#include "../skynodes/nodes/linenode.h"
#include "../skynodes/skypolygonnode.h"
#include "../skynodes/trixelnode.h"

MilkyWayItem::MilkyWayItem(MilkyWay *mwComp, RootNode *rootNode)
    :SkyItem(LabelsItem::label_t::NO_LABEL, rootNode), m_filled(Options::fillMilkyWay()), m_MWComp(mwComp)
{
    initialize();
}

void MilkyWayItem::initialize() {
    LineListHash *trixels = m_MWComp->polyIndex();
    while(QSGNode *n = firstChild()) { removeChildNode(n); delete n; }

    QHash< Trixel, LineListList *>::const_iterator i = trixels->cbegin();
    QList<LineList *> addedLines;
    while( i != trixels->cend()) {
        LineListList *linesList = *i;

        if(linesList->size()) {
            TrixelNode *trixel = new TrixelNode(i.key());
            appendChildNode(trixel);

            QColor schemeColor = KStarsData::Instance()->colorScheme()->colorNamed("MWColor");
            for(int c = 0; c < linesList->size(); ++c) {
                LineList *list = linesList->at(c);
                if(!addedLines.contains(list)) {
                    if(m_filled) {
                        SkyPolygonNode *poly = new SkyPolygonNode(list);
                        schemeColor.setAlpha(0.7*255);
                        poly->setColor(schemeColor);
                        trixel->appendChildNode(poly);
                    } else {
                        LineNode * ln = new LineNode(list, m_MWComp->skipList(list), schemeColor, 3, Qt::SolidLine);
                        trixel->appendChildNode(ln);
                    }
                    addedLines.append(list);
                }
            }
        }
        ++i;
    }
}

void MilkyWayItem::update() {
    if(m_MWComp->selected()) {
        show();
        QSGNode *n = firstChild();

        DrawID   drawID   = SkyMesh::Instance()->drawID();
        UpdateID updateID = KStarsData::Instance()->updateID();

        QColor schemeColor = KStarsData::Instance()->colorScheme()->colorNamed("MWColor");

        if(Options::fillMilkyWay() != m_filled) {
            m_filled = Options::fillMilkyWay();
            initialize();
        }

        while(n != 0) {
            TrixelNode * trixel = static_cast<TrixelNode *>(n);
            trixel->show();
            n = n->nextSibling();

            QSGNode *l = trixel->firstChild();
            while(l != 0) {
                if(m_filled) {
                    SkyPolygonNode *polygon = static_cast<SkyPolygonNode *>(l);
                    LineList * lineList = polygon->lineList();
                    polygon->setColor(schemeColor);

                    if ( lineList->drawID == drawID ) {
                        polygon->hide();
                        l = l->nextSibling();
                        continue;
                    }

                    lineList->drawID = drawID;
                    if ( lineList->updateID != updateID )
                        m_MWComp->JITupdate( lineList );

                    polygon->update();
                } else {
                    LineNode * lines = static_cast<LineNode *>(l);
                    QColor schemeColor = KStarsData::Instance()->colorScheme()->colorNamed("MWColor");
                    lines->setColor(schemeColor);

                    LineList * lineList = lines->lineList();
                    if ( lineList->drawID == drawID ) {
                        lines->hide();
                        l = l->nextSibling();
                        continue;
                    }
                    lineList->drawID = drawID;
                    if ( lineList->updateID != updateID )
                        m_MWComp->JITupdate( lineList );

                    lines->updateGeometry();
                }
                l = l->nextSibling();
            }
        }
    } else {
        hide();
    }
}




