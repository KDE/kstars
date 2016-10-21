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

/**
 * @short Convenience class that represents trixel in SkyMapLite. It should be used as a parent for
 * nodes that represent SkyObjects indexed by HTMesh
 */
class TrixelNode : public SkyOpacityNode {
public:
    /** Constructor. **/
    TrixelNode(Trixel trixel);

    /** m_hideCount is a counter of how much updates of SkyMapLite this trixel remained
        hidden. Used to reduce memory consumption**/
    inline int hideCount() { return m_hideCount; }

    /** Whenever the corresponding trixel is visible, m_hideCount is reset */
    inline void resetHideCount() { m_hideCount = 0; }

    void virtual hide() override;
    void virtual show() override;

    inline Trixel trixelID() { return m_trixel; }

    /**
     * @short m_nodes - holds SkyNodes with corresponding SkyObjects
     */
    QLinkedList<QPair<SkyObject *, SkyNode *>> m_nodes;

    /** @short Delete all childNodes and remove nodes from pairs in m_nodes **/
    virtual void deleteAllChildNodes();

private:
    Trixel m_trixel;
    int m_hideCount;
};


#endif
