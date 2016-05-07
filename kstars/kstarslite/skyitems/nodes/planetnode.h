/** *************************************************************************
                          planetnode.h  -  K Desktop Planetarium
                             -------------------
    begin                : 05/05/2016
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
#ifndef PLANETNODE_H_
#define PLANETNODE_H_
#include <QSGNode>
#include "pointnode.h"

class QSGSimpleTextureNode;
class QImage;
class KSPlanetBase;
class PlanetItemNode;

/** @class PlanetNode
 *
 * A QSGNode derived class used as a container for holding two other nodes PointNode
 * and QSGSimpleTextureNode that are displayed depending on the conditions (zoom level,
 * user options)
 *
 *@short A container for PointNode and QSGSimpleTextureNode used for displaying planet
 *@author Artem Fedoskin
 *@version 1.0
 */

class PlanetNode : public QSGTransformNode {
public:
    /**
     * @brief Constructor
     * @param planet used in PlanesItem to update position of PlanetNode
     * @param parentNode used by PointNode to get textures from cache
     */
    PlanetNode(KSPlanetBase* planet, PlanetItemNode* parentNode);
    /**
     * @short setPointSize updates the size of m_point
     * @param size new size of m_point
     */
    void setPointSize(float size);
    /**
     * @short setPlanetPicSize updates the size of m_planetPic
     * @param size new size of m_planetPic
     */
    void setPlanetPicSize(float size);
    /**
     * @short makes m_planetPic invisible and m_point visible
     */
    void showPoint();
    /**
     * @short hides m_point and shows m_planetPic
     */
    void showPlanetPic();
    /**
     * @short hides both nodes
     */
    void hide();
    /**
     * @short changePos changes the position of both nodes
     * @param pos new position
     */
    void changePos(QPointF pos);
    /**
     * @return the planet associated with this PlanetNode
     */
    inline KSPlanetBase* planet() { return m_planet; }
private:
    PointNode* m_point;

    // This opacity node is used to hide m_planetPic. m_point is subclass of QSGOpacityNode so it needs
    // no explicit opacity node here.
    QSGOpacityNode* m_planetOpacity;
    QSGSimpleTextureNode* m_planetPic;

    KSPlanetBase* m_planet;
};


#endif
