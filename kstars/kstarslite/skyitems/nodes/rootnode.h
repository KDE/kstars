 
/** *************************************************************************
                          rootnode.h  -  K Desktop Planetarium
                             -------------------
    begin                : 14/05/2016
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
#ifndef ROOTNODE_H_
#define ROOTNODE_H_
#include <QSGClipNode>
#include <QPolygonF>

class QSGSimpleTextureNode;
class QImage;
class QSGTexture;
class SkyMapLite;

/** @class RootNode
 *
 * A QSGClipNode derived class used as a container for holding nodes and for clipping. Upon construction
 * RootNode generates all textures that are used by stars.
 *
 *@short A container for nodes that holds collection of textures for stars and provides clipping
 *@author Artem Fedoskin
 *@version 1.0
 */

class RootNode : public QSGClipNode {
public:
    RootNode();
    /**
     * @short returns cached texture from textureCache
     * @param size size of the star
     * @param spType spectral class
     * @return cached QSGTexture from textureCache
     */
    QSGTexture* getCachedTexture(int size, char spType);

    /**
     * @brief Adds node to m_skyNodes and node tree
     * @param skyNode pointer to skyNode that has to be added
     */
    void appendSkyNode(QSGNode * skyNode);

    /**
     * Triangulates clipping polygon provided by Projection system
     * @short updates clipping geometry using triangles data in SkyMapLite
     */
    void updateClipPoly();

    /**
     * @return number of SkyNodes in m_skyNodes
     */
    inline int skyNodesCount() { return m_skyNodes.length(); }

    /**
     * @short returns a SkyNode in m_skyNodes with i index
     * @param i index of SkyNode
     * @return desired SkyNode
     */
    inline QSGNode * skyNodeAtIndex(int i) { return m_skyNodes[i]; }

private:
    /**
     * @short initializes textureCache with cached images of stars in SkyMapLite
     */
    void genCachedTextures();
    QVector<QVector<QSGTexture *>> m_textureCache;
    SkyMapLite *m_skyMapLite;

    QPolygonF m_clipPoly;
    QSGGeometryNode *m_polyNode;
    QSGGeometry *m_polyGeometry;
    //To hold nodes that represent sky objects
    QVector<QSGNode *> m_skyNodes;
};
#endif
