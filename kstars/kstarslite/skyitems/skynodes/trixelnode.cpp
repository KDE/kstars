/** *************************************************************************
                          TrixelNode.cpp  -  K Desktop Planetarium
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

#include <QImage>
#include <QQuickWindow>

#include "kstarsdata.h"
#include "trixelnode.h"
#include "skyobject.h"
#include "nodes/linenode.h"
#include "Options.h"
#include "skymesh.h"
#include <QSGOpacityNode>

TrixelNode::TrixelNode(Trixel trixelId, LineListList *linesList)
    :SkyNode(), trixel(trixelId), m_linesLists(linesList), m_opacity(new QSGOpacityNode)
{
    appendChildNode(m_opacity);
    for(int i = 0; i < m_linesLists->size(); ++i) {
        LineNode * ln = new LineNode(m_linesLists->at(i));
        m_opacity->appendChildNode(ln);
    }
}

void TrixelNode::setStyle(QString color, int width, Qt::PenStyle style) {
    QSGNode *n = m_opacity->firstChild();
    while(n != 0) {
        LineNode *ln = static_cast<LineNode *>(n);
        ln->setStyle(KStarsData::Instance()->colorScheme()->colorNamed( color ), width, style);
        n = n->nextSibling();
    }
}

void TrixelNode::update() {
    show();
    DrawID   drawID   = SkyMesh::Instance()->drawID();
    //UpdateID updateID = KStarsData::Instance()->updateID();
    //for(int i = 0; i < m_opacity->childCount(); ++i) {
    QSGNode *n = m_opacity->firstChild();
    while(n != 0) {
        LineNode * lines = static_cast<LineNode *>(n);
        n = n->nextSibling();

        LineList * lineList = lines->lineList();
        if ( lineList->drawID == drawID ) {
            lines->hide();
            continue;
        }
        lineList->drawID = drawID;
        lines->updateGeometry();
    }
}
