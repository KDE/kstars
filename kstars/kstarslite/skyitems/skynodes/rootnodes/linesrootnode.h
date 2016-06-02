 
/** *************************************************************************
                          LinesRootNode.h  -  K Desktop Planetarium
                             -------------------
    begin                : 14/05/2016
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
#ifndef LINESROOTNODE_H_
#define LINESROOTNODE_H_
#include "linelistindex.h"
#include <QSGNode>

/** @class LinesRootNode
 *
 * A QSGClipNode derived class used as a container for holding pointers to nodes and for clipping.
 * Upon construction LinesRootNode generates all textures that are used by PointNode.
 *
 *@short A container for nodes that holds collection of textures for stars and provides clipping
 *@author Artem Fedoskin
 *@version 1.0
 */

class LinesRootNode : public QSGNode { //Clipping under construction
public:
    LinesRootNode();
    void addLinesComponent(LineListIndex * linesList, QString color, int width, Qt::PenStyle style);
    void update();
private:
    QMap<QSGOpacityNode *, LineListIndex *> m_lineIndexes;
    QVector<QString> m_colors;
    QVector<int> m_widths;
    QVector<Qt::PenStyle> m_styles;
};
#endif
