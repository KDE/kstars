/** *************************************************************************
                          satellitenode.h  -  K Desktop Planetarium
                             -------------------
    begin                : 25/06/2016
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
#ifndef SATELLITENODE_H_
#define SATELLITENODE_H_
#include "skynode.h"

class PolyNode;
class Satellite;
class PointNode;
class QSGFlatColorMaterial;
class LabelNode;

    /** @class SatelliteNode
     *
     *@short A SkyNode derived class that represents satellite
     *@author Artem Fedoskin
     *@version 1.0
     */

class SatelliteNode : public SkyNode {
public:
    /**
     * @short Constructor.
     * @param sat - satellite that is represented by this node
     * @param rootNode - pointer to the top parent node
     */
    SatelliteNode(Satellite* sat, RootNode *rootNode);

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

    void changePos(QPointF pos);

    inline Satellite *sat() { return m_sat; }

private:
    Satellite *m_sat;
    RootNode *m_rootNode;

    SkyOpacityNode *m_linesOpacity;
    QSGGeometryNode *m_lines;

    LabelNode *m_label;

    QSGFlatColorMaterial *m_material;
    QSGGeometry *m_geometry;

    PointNode *m_point;
};


#endif


