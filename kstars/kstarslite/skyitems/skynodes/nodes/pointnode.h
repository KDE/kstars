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
class SkyMapLite;

/** @class PointNode
 *
 * A QSGOpacityNode derived class used for representing stars and planets as stars. Upon
 * construction loads the texture of star cached in parentNode
 *
 *@short QSGOpacityNode derived class that represents stars and planets using cached QSGTexture
 *@author Artem Fedoskin
 *@version 1.0
 */

class RootNode;

class PointNode : public QSGOpacityNode  {
public:
    /**
     * @short Constructor
     * @param spType spectral type
     * @param parentNode pointer to the top parent node, which holds texture cache
     * @param size initial size of PointNode
     */
    PointNode(RootNode* rootNode, char spType = 'A', float size = 1);
    /**
     * @short setSize update size of PointNode with the given parameter
     * @param size new size of PointNode
     */
    void setSize(float size);

    inline QSizeF size() const { return texture->rect().size(); }

    void show();
    void hide();
private:
    char spType;
    QSGSimpleTextureNode *texture;
    //parentNode holds texture cache
    RootNode* m_rootNode;
    float m_size;
};

#endif
