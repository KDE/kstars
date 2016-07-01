/** *************************************************************************
                          trixelnode.h  -  K Desktop Planetarium
                             -------------------
    begin                : 01/07/2016
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
#ifndef TRIXELNODE_H_
#define TRIXELNODE_H_

#include "../skyopacitynode.h"
#include "typedef.h"

class SkyObject;
class SkyNode;

class TrixelNode : public SkyOpacityNode {
public:
    Trixel m_trixel;

    QLinkedList<QPair<SkyObject *, SkyNode *>> m_nodes;

    inline int hideCount() { return m_hideCount; }
    inline void resetHideCount() { m_hideCount = 0; }

    void virtual hide() override;
    void virtual show() override;
private:
    int m_hideCount;
};


#endif
