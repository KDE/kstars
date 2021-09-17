/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "skynode.h"

class PolyNode;
class Satellite;
class PointNode;
class QSGFlatColorMaterial;
class LabelNode;

/**
 * @class SatelliteNode
 *
 * @short A SkyNode derived class that represents satellite
 * @author Artem Fedoskin
 * @version 1.0
 */

class SatelliteNode : public SkyNode
{
  public:
    /**
     * @short Constructor.
     * @param sat - satellite that is represented by this node
     * @param rootNode - pointer to the top parent node
     */
    SatelliteNode(Satellite *sat, RootNode *rootNode);

    /**
     * @short Update position and visibility of satellite.
     * We also check user settings (Options::drawSatellitesLikeStars()) and based on that draw satellite
     * either like star or with lines
     */
    virtual void update() override;
    virtual void hide() override;

    /**
     * @short Initialize m_lines (if not already) to draw satellite with lines
     */
    void initLines();

    /**
     * @short Initialize m_point (if not already) to draw satellite as a star
     */
    void initPoint();

    virtual void changePos(QPointF pos) override;

    inline Satellite *sat() { return m_sat; }

  private:
    Satellite *m_sat;
    RootNode *m_rootNode;

    QSGGeometryNode *m_lines { nullptr };

    LabelNode *m_label { nullptr };

    QSGFlatColorMaterial *m_material { nullptr };
    QSGGeometry *m_geometry { nullptr };

    PointNode *m_point { nullptr };
};
