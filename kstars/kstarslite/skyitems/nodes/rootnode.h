 
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
class SkyNode;

/** @class RootNode
 *
 * A QSGClipNode derived class used as a container for holding pointers to nodes and for clipping.
 * Upon construction RootNode generates all textures that are used by PointNode.
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
     * @short triangulates clipping polygon provided by Projection system
     */
    void updateClipPoly();

    /**
     * @return number of SkyNodes in m_skyNodes
     */
    inline int skyNodesCount() { return m_skyNodes.length(); }

    /**
     * @short returns a SkyNode in m_skyNodes with index i
     * @param i index of SkyNode
     * @return desired SkyNode
     */
    inline SkyNode *skyNodeAtIndex(int i) { return m_skyNodes[i]; }

    /**
     * @brief Adds node to m_skyNodes and node tree
     * @param skyNode pointer to skyNode that has to be added
     */
    void appendSkyNode(SkyNode *skyNode);

    void prependSkyNode(SkyNode *skyNode);

    /**
     * @short remove given skyNode from m_skyNodes and a node tree
     * @param skyNode pointer to skyNode that needs to be deleted
     */
    void removeSkyNode(SkyNode *skyNode);

    /**
     * @short deletes all SkyNodes from m_skyNodes and node tree
     */
    void removeAllSkyNodes();
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
    QVector<SkyNode *> m_skyNodes;
    bool m_hidden;
};
#endif
