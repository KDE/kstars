/** *************************************************************************
                          pointnode.h  -  K Desktop Planetarium
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
#ifndef POINTNODE_H_
#define POINTNODE_H_
#include <QSGSimpleTextureNode>

class PlanetItemNode;

/** @class PointNode
 *
 * A QSGSimpleTextureNode derived class used for representing stars and planets as stars. Upon
 * construction loads the texture of star, cached as a QPixmap in SkyMapLite
 *
 *@short QSGSimpleTextureNode derived class that represents stars and planets using cached QPixmap
 *@author Artem Fedoskin
 *@version 1.0
 */

class PointNode : public QSGOpacityNode  {
public:
    /**
     * @short Constructor
     * @param spType spectral type
     * @param parentNode pointer to top parent node, which holds texture cache
     * @param size initial size of PointNode
     */
    PointNode(char spType, PlanetItemNode* parentNode, float size = 1);
    /**
     * @short setSize update size of PointNode with the given parameter
     * @param size new size of PointNode
     */
    void setSize(float size);
    /**
     * @short changePos changes position of PointNode to the one specified by pos
     * @param pos new position
     */
    void changePos(QPointF pos);
private:
    char spType;
    QSGSimpleTextureNode *texture;
    // This pointer is important because parentNode holds texture cache
    PlanetItemNode* parentNode;
};

#endif
