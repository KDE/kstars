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
 *@version 1.0
 */

class SatelliteNode : public SkyNode {
public:
    SatelliteNode(Satellite* sat, RootNode *rootNode);

    void update();
    virtual void hide() override;
    void initLines();
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


