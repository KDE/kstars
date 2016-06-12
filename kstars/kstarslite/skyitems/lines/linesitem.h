/** *************************************************************************
                          asteroidsitem.h  -  K Desktop Planetarium
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
#ifndef LINESITEM_H_
#define LINESITEM_H_

#include "../skyitem.h"

class KSAsteroid;
class LineListIndex;

class LinesItem : public SkyItem {
public:

    LinesItem(RootNode *rootNode);

    void addLinesComponent(LineListIndex *linesComp, QString color, int width, Qt::PenStyle style);

    virtual void update();
private:
    QMap<QSGOpacityNode *,LineListIndex *> m_lineIndexes;
};
#endif

