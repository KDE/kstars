/** *************************************************************************
                          planetitemnode.h  -  K Desktop Planetarium
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
#ifndef PLANETITEMNODE_H_
#define PLANETITEMNODE_H_
#include <QSGNode>

class QSGSimpleTextureNode;
class QImage;
class PlanetNode;
class QSGTexture;

/** @class PlanetItemNode
 *
 * A QSGNode derived class used as a container for holding PlanetNodes. Upon construction
 * PlanetItemNode generates all textures that are used by stars
 *
 *@short A container for PlanetNodes that holds collection of textures for stars
 *@author Artem Fedoskin
 *@version 1.0
 */

class PlanetItemNode : public QSGNode {
public:
    PlanetItemNode();
    /**
     * @short returns cached texture from textureCache
     * @return cached QSGTexture from textureCache
     */
    QSGTexture* getCachedTexture(int size, char spType);

private:
    /**
     * @short initializes textureCache with cached images of stars in SkyMapLite
     */
    void genCachedTextures();
    QVector<QVector<QSGTexture *>> textureCache;
};
#endif
