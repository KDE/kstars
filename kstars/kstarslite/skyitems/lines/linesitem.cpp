/** *************************************************************************
                          linesitem.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 1/06/2016
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

#include "linesitem.h"
#include "linelist.h"
#include "linelistindex.h"
#include "../skynodes/nodes/linenode.h"
#include "../skynodes/trixelnode.h"

LinesItem::LinesItem(RootNode *rootNode)
    :SkyItem(LabelsItem::label_t::NO_LABEL, rootNode)
{

}

LineIndexNode::LineIndexNode(QString color)
    :schemeColor(color)
{

}

void LinesItem::addLinesComponent(LineListIndex *linesComp, QString color, int width, Qt::PenStyle style) {
    LineIndexNode *node = new LineIndexNode(color);
    appendChildNode(node);

    m_lineIndexes.insert(node, linesComp);
    LineListHash *list = linesComp->lineIndex();
    //Sort by trixels
    QMap<Trixel, LineListList*> trixels;

    QHash<Trixel, LineListList* >::const_iterator s = list->cbegin();
    while(s != list->cend()) {
        trixels.insert(s.key(), *s);
        s++;
    }

    QMap<Trixel, LineListList* >::const_iterator i = trixels.cbegin();
    QList<LineList *> addedLines;
    while( i != trixels.cend()) {
        LineListList *linesList = *i;

        if(linesList->size()) {
            TrixelNode *trixel = new TrixelNode(i.key());
            node->appendChildNode(trixel);

            QColor schemeColor = KStarsData::Instance()->colorScheme()->colorNamed(color);
            for(int c = 0; c < linesList->size(); ++c) {
                LineList *list = linesList->at(c);
                if(!addedLines.contains(list)) {
                    trixel->appendChildNode(new LineNode(linesList->at(c), 0, schemeColor, width, style));
                    addedLines.append(list);
                }
            }
        }
        ++i;
    }
}

void LinesItem::update() {
    QMap< LineIndexNode *, LineListIndex *>::iterator i = m_lineIndexes.begin();

    SkyMapLite *map = SkyMapLite::Instance();

    double radius = map->projector()->fov();
    if ( radius > 90.0 ) radius = 90.0;

    UpdateID updateID = KStarsData::Instance()->updateID();

    while( i != m_lineIndexes.end()) {
        LineIndexNode * node = i.key();
        QColor schemeColor = KStarsData::Instance()->colorScheme()->colorNamed(node->getSchemeColor());
        if(i.value()->selected()) {
            node->show();

            QSGNode *n = node->firstChild();
            while(n != 0) {
                TrixelNode * trixel = static_cast<TrixelNode *>(n);
                    trixel->show();

                    QSGNode *l = trixel->firstChild();
                    while(l != 0) {
                        LineNode * lines = static_cast<LineNode *>(l);
                        lines->setColor(schemeColor);
                        l = l->nextSibling();

                        LineList * lineList = lines->lineList();
                        if ( lineList->updateID != updateID )
                            i.value()->JITupdate( lineList );

                        lines->updateGeometry();
                    }
                n = n->nextSibling();
            }
        } else {
            node->hide();
        }
        ++i;
    }
}

