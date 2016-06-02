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

#include "skynodes/trixelnode.h"
#include "linesitem.h"
#include "linelist.h"
#include "skynodes/rootnodes/linesrootnode.h"

LinesItem::LinesItem(QQuickItem* parent)
    :SkyItem(parent)
{
    Options::setHideCBounds(true);
    Options::setShowCBounds(true);
    Options::setShowCLines(true);
    Options::setHideOnSlew(true);
    Options::setHideCLines(false);
    Options::setShowSolarSystem(true);

    Options::setShowEcliptic(true);
    Options::setShowEquator(true);
    Options::setShowEquatorialGrid(true);
    Options::setShowHorizontalGrid(true);
    Options::setHideGrids(false);
}

void LinesItem::addLinesComponent(LineListIndex *linesComp, QString color, int width, Qt::PenStyle style) {
    m_lineIndexes.append(QPair<bool, LineListIndex *>(true,linesComp));
    m_colors.append(color);
    m_widths.append(width);
    m_styles.append(style);
}

QSGNode* LinesItem::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *updatePaintNodeData) {

    Q_UNUSED(updatePaintNodeData);

    LinesRootNode *n = static_cast<LinesRootNode *>(oldNode);
    QRectF rect = boundingRect();

    if (rect.isEmpty()) {
        delete n;
        return 0;
    }

    if(!n) {
        n = new LinesRootNode; // If no RootNode exists create one
    }

    for(int i = 0; i < m_lineIndexes.size(); ++i) {
        QPair<bool, LineListIndex *> *p = &m_lineIndexes[i];
        if(p->first) {
            n->addLinesComponent(p->second,m_colors[i],m_widths[i],m_styles[i]);
            p->first = false;
        }
    }

    //Update clipping geometry. If m_clipPoly in SkyMapLite wasn't changed, geometry is not updated
    //n->updateClipPoly();

    n->update();
    return n;
}

