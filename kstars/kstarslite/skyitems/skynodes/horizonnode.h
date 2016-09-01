/** *************************************************************************
                          horizonnode.h  -  K Desktop Planetarium
                             -------------------
    begin                : 28/05/2016
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
#ifndef HORIZONNODE_H_
#define HORIZONNODE_H_
#include "skynode.h"

class PolyNode;

/** @class HorizonNode
 *
 *@version 1.0
 */

class HorizonNode : public SkyNode {
public:
    HorizonNode(QList<SkyPoint*>& pointList);

    virtual void update() override;
    virtual void hide() override;

private:
    QList<SkyPoint*>& m_pointList;
    PolyNode *m_polygonNode;
};


#endif


