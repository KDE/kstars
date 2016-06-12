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
#ifndef EQUATORITEM_H_
#define EQUATORITEM_H_

#include "../skyitem.h"
#include "equator.h"

class KSAsteroid;
class LineListIndex;

class EquatorItem : public SkyItem {
public:

    EquatorItem(Equator *equatorComp, RootNode *rootNode);

    virtual void update();
private:
    Equator *m_equatorComp;
    QMap<SkyPoint *, LabelNode *> m_compassLabels;
};
#endif

