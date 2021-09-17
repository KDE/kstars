/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "skynode.h"

class QSGFlatColorMaterial;
class PointNode;
class PolyNode;
class Supernova;

/**
 * @class SupernovaNode
 *
 * @short A SkyNode derived class that represents supernova
 * @author Artem Fedoskin
 * @version 1.0
 */
class SupernovaNode : public SkyNode
{
  public:
    /**
     * @short Constructor.
     * @param snova - pointer to supernova that needs to be represented by this node
     */
    explicit SupernovaNode(Supernova *snova);

    /**
     * @short Update position and visibility of supernova. Initialize m_lines if not already done
     */
    virtual void update() override;

    virtual void changePos(QPointF pos) override;

    inline Supernova *snova() { return m_snova; }

  private:
    Supernova *m_snova { nullptr };

    QSGGeometryNode *m_lines { nullptr };

    QSGFlatColorMaterial *m_material { nullptr };
    QSGGeometry *m_geometry { nullptr };
};
