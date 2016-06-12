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
#ifndef ECLIPTICITEM_H_
#define ECLIPTICITEM_H_

#include "../skyitem.h"

class Ecliptic;

class EclipticItem : public SkyItem {
public:

    EclipticItem(Ecliptic *eclipticComp, RootNode *rootNode);

    virtual void update();
private:
    Ecliptic *m_eclipticComp;
    QMap<SkyPoint *, LabelNode *> m_compassLabels;
};
#endif

