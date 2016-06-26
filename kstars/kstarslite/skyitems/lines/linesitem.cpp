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

LinesItem::LinesItem(RootNode *rootNode)
    :SkyItem(LabelsItem::label_t::NO_LABEL, rootNode)
{

}

void LinesItem::addLinesComponent(LineListIndex *linesComp, QString color, int width, Qt::PenStyle style) {
    LineIndexNode *node = new LineIndexNode;
    appendChildNode(node);

    m_lineIndexes.insert(node, linesComp);
    LineListHash *trixels = linesComp->lineIndex();

    QHash< Trixel, LineListList *>::const_iterator i = trixels->begin();
    while( i != trixels->end()) {
        LineListList *linesList = *i;

        if(linesList->size()) {
            TrixelNode *trixel = new TrixelNode;
            node->appendChildNode(trixel);

            QColor schemeColor = KStarsData::Instance()->colorScheme()->colorNamed(color);
            for(int c = 0; c < linesList->size(); ++c) {
                //LineNode * ln = new LineNode(linesList->at(c), schemeColor, width, style);
                /*trixel->appendChildNode(ln);*/
                trixel->appendChildNode(new LineNode(linesList->at(c), 0, schemeColor, width, style));
            }
        }
        ++i;
    }
}

void LinesItem::update() {
    QMap< LineIndexNode *, LineListIndex *>::const_iterator i = m_lineIndexes.begin();
    while( i != m_lineIndexes.end()) {
        //QVector<Trixel> visTrixels;
        SkyMesh * mesh = SkyMesh::Instance();
        SkyMapLite *map = SkyMapLite::Instance();
        double radius = map->projector()->fov();
        if ( radius > 180.0 )
            radius = 180.0;
        if(mesh) {
            //mesh->aperture(map->focus(), radius);
        }

        /*MeshIterator region (mesh,DRAW_BUF);
        while ( region.hasNext() ) {
            visTrixels.append(region.next());
        }*/

        DrawID   drawID   = SkyMesh::Instance()->drawID();
        UpdateID updateID = KStarsData::Instance()->updateID();

        LineIndexNode * node = i.key();
        if(i.value()->selected()) {
            node->show();

            QSGNode *n = node->firstChild();
            while(n != 0) {
                TrixelNode * trixel = static_cast<TrixelNode *>(n);
                trixel->show();

                n = n->nextSibling();

                //if(visTrixels.contains(c)) {

                QSGNode *l = trixel->firstChild();
                while(l != 0) {
                    LineNode * lines = static_cast<LineNode *>(l);
                    l = l->nextSibling();

                    LineList * lineList = lines->lineList();
                    if ( lineList->drawID == drawID ) {
                        lines->hide();
                        continue;
                    }
                    if ( lineList->updateID != updateID )
                        i.value()->JITupdate( lineList );

                    lineList->drawID = drawID;
                    lines->updateGeometry();
                }

                /* } else {
                    trixel->hide();
                }*/
            }
        } else {
            node->hide();
        }
        ++i;
    }
}

