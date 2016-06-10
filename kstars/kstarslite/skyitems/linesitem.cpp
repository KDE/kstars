/** *************************************************************************
                          EquatorItem.cpp  -  K Desktop Planetarium
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

#include "Options.h"
#include "projections/projector.h"
#include <QSGNode>

#include "skynodes/trixelnode.h"
#include "linesitem.h"
#include "linelist.h"
#include "linelistindex.h"
#include "skynodes/nodes/linenode.h"

LinesItem::LinesItem(RootNode *rootNode)
    :SkyItem(LabelsItem::label_t::RUDE_LABEL, rootNode)
{
    //Under construction
    Options::setShowCBounds(true);
    Options::setShowCLines(true);
    Options::setShowSolarSystem(true);
    Options::setShowEcliptic(true);
    Options::setShowEquator(true);
    Options::setShowEquatorialGrid(true);
    Options::setShowHorizontalGrid(true);
    Options::setShowGround(true);

    //Labels
    Options::setShowCometNames(true);
    Options::setShowAsteroidNames(true);
    Options::setShowAsteroids(true);

    Options::setAsteroidLabelDensity(10000);
    Options::setMagLimitAsteroid(-10);

    Options::setHideCBounds(true);
    Options::setHideCLines(false);
    Options::setHideOnSlew(true);
    Options::setHideGrids(false);

    Options::setRunClock(false);
}

void LinesItem::addLinesComponent(LineListIndex *linesComp, QString color, int width, Qt::PenStyle style) {
    QSGOpacityNode *node = new QSGOpacityNode;
    appendChildNode(node);

    m_lineIndexes.insert(node, linesComp);
    LineListHash *trixels = linesComp->lineIndex();

    QHash< Trixel, LineListList *>::const_iterator i = trixels->begin();
    while( i != trixels->end()) {

        TrixelNode *trixel = new TrixelNode(i.key(), i.value());
        node->appendChildNode(trixel);
        trixel->setStyle(color, width, style);
        ++i;
    }
}

void LinesItem::update() {
    QMap< QSGOpacityNode *, LineListIndex *>::const_iterator i = m_lineIndexes.begin();
    while( i != m_lineIndexes.end()) {
        QVector<Trixel> visTrixels;
        SkyMesh * mesh = SkyMesh::Instance();
        SkyMapLite *map = SkyMapLite::Instance();
        double radius = map->projector()->fov();
        if ( radius > 180.0 )
            radius = 180.0;
        if(mesh) {
            //mesh->aperture(map->focus(), radius);
        }

        MeshIterator region (mesh,DRAW_BUF);
        while ( region.hasNext() ) {
            visTrixels.append(region.next());
        }

        QSGOpacityNode * node = i.key();
        if(i.value()->selected()) {
            node->setOpacity(1);
            //for(int c = 0; c < node->childCount(); ++c) {
            QSGNode *n = node->firstChild();
            while(n != 0) {
                TrixelNode * trixel = static_cast<TrixelNode *>(n);
                n = n->nextSibling();
                //if(visTrixels.contains(c)) {
                    trixel->update();
               /* } else {
                    trixel->hide();
                }*/
            }
        } else {
            node->setOpacity(0);
        }
        node->markDirty(QSGNode::DirtyOpacity);
        ++i;
    }
}

