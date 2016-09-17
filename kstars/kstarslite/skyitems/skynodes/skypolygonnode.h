/** *************************************************************************
                          skypolygonnode.h  -  K Desktop Planetarium
                             -------------------
    begin                : 23/06/2016
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
#ifndef SKYPOLYGONNODE_H_
#define SKYPOLYGONNODE_H_
#include "skynode.h"

class PolyNode;

/** @class HorizonNode
 *
 *@version 1.0
 */

class SkyPolygonNode : public SkyNode {
public:
    SkyPolygonNode(LineList* list);

    void update(bool forceClip = true);
    virtual void hide() override;
    LineList *lineList() { return m_list; }
    void setColor(QColor color);
private:
    LineList *m_list;
    PolyNode *m_polygonNode;
};


#endif


