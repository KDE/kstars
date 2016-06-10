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
#include "skynode.h"

class QSGSimpleTextureNode;
class QImage;
class KSPlanetBase;
class RootNode;
class PointNode;
class LabelNode;

/** @class PlanetNode
 *
 * A SkyNode derived class used as a container for holding two other nodes: PointNode
 * and QSGSimpleTextureNode(displays object as image) that are displayed depending on the conditions
 * (zoom level, user options)
 *
 *@short A container for PointNode and QSGSimpleTextureNode used for displaying some solar system objects
 *@author Artem Fedoskin
 *@version 1.0
 */

class PlanetNode : public SkyNode {
public:
    /**
     * @brief Constructor
     * @param planet used in PlanesItem to update position of PlanetNode
     * @param parentNode used by PointNode to get textures from cache
     */
    PlanetNode(KSPlanetBase* planet, RootNode* parentNode);

    /**
     * @short updates the size of m_point
     * @param size new size of m_point
     */
    void setPointSize(float size);
    /**
     * @short updates the size of m_planetPic
     * @param size new size of m_planetPic
     */
    void setPlanetPicSize(float size);
    /**
     * @short hides m_planetPic and shows m_point
     */
    void showPoint();
    /**
     * @short hides m_point and shows m_planetPic
     */
    void showPlanetPic();
    /**
     * @short changePos changes the position m_point and m_planetPic
     * @param pos new position
     */
    virtual void changePos(QPointF pos) override;

    virtual void update() override;

    virtual void hide() override;
private:
    PointNode *m_point;

    // This opacity node is used to hide m_planetPic. m_point is subclass of QSGOpacityNode so it needs
    // no explicit opacity node here.
    QSGOpacityNode *m_planetOpacity;
    QSGSimpleTextureNode *m_planetPic;
    LabelNode *m_label;
};


#endif
