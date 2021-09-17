/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "typedef.h"
#include "../skyopacitynode.h"

#include <QLinkedList>

class SkyObject;
class SkyNode;

/**
 * @short Convenience class that represents trixel in SkyMapLite. It should be used as a parent for
 * nodes that represent SkyObjects indexed by HTMesh
 */
class TrixelNode : public SkyOpacityNode
{
  public:
    /** Constructor **/
    explicit TrixelNode(const Trixel &trixel);

    /**
     * m_hideCount is a counter of how much updates of SkyMapLite this trixel remained
     * hidden. Used to reduce memory consumption
     **/
    inline int hideCount() { return m_hideCount; }

    /** Whenever the corresponding trixel is visible, m_hideCount is reset */
    inline void resetHideCount() { m_hideCount = 0; }

    void virtual hide() override;
    void virtual show() override;

    inline Trixel trixelID() { return m_trixel; }

    /** m_nodes - holds SkyNodes with corresponding SkyObjects */
    QLinkedList<QPair<SkyObject *, SkyNode *>> m_nodes;

    /** @short Delete all childNodes and remove nodes from pairs in m_nodes **/
    virtual void deleteAllChildNodes();

  private:
    Trixel m_trixel;
    int m_hideCount { 0 };
};
