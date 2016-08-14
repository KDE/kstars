/** *************************************************************************
                          pointsourcenode.h  -  K Desktop Planetarium
                             -------------------
    begin                : 16/05/2016
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
#ifndef POINTSOURCENODE_H_
#define POINTSOURCENODE_H_
#include "skynode.h"
#include "../labelsitem.h"

class PlanetItemNode;
class SkyMapLite;
class PointNode;
class LabelNode;

/** @class PointSourceNode
 *
 * A SkyNode derived class used for displaying PointNode with coordinates provided by SkyObject.
 *
 *@short A SkyNode derived class that represents stars and objects that are drawn as stars
 *@author Artem Fedoskin
 *@version 1.0
 */

class RootNode;

class PointSourceNode : public SkyNode  {
public:
    /**
     * @short Constructor
     * @param skyObject pointer to SkyObject that has to be displayed on SkyMapLite
     * @param parentNode pointer to the top parent node, which holds texture cache
     * @param spType spectral class of PointNode
     * @param size initial size of PointNode
     */
    PointSourceNode(SkyObject * skyObject, RootNode * parentNode,
                    LabelsItem::label_t labelType = LabelsItem::label_t::STAR_LABEL, char spType = 'A', float size = 1, short trixel = -1);

    virtual ~PointSourceNode();

    /** @short Get the width of a star of magnitude mag */
    float starWidth(float mag) const;

    /**
     * @short updatePoint initializes PointNode if not done that yet. Makes it visible and updates
     * its size.
     * By using this function we can save some memory because m_point is created only when this
     * PointSourceNode becomes visible.
     */
    void updatePoint();

    /**
     * @short changePos changes the position m_point
     * @param pos new position
     */
    virtual void changePos(QPointF pos) override;

    /**
     * @short updatePos updates position of this node and its label. Initializes label if needed
     * The reason behind this function is that in StarItem we are already checking the visibility of star
     * to decide whether we need to create this node or no. So to avoid calculating the same thing twice
     * we set position of object directly. Also through this function StarItem sets the visibility of label
     * @param pos position of this node on SkyMapLite
     * @param drawLabel true if label has to be drawn
     */
    void updatePos(QPointF pos, bool drawLabel);

    /**
     * @short update updates coordinates of this node based on the visibility of its SkyObject
     */
    virtual void update() override;

    /**
     * @short hides this node and its label. m_point is hided without explicitly doing this because
     * it is a child node of m_opacity inherited from SkyNode
     */
    virtual void hide() override;
private:
    PointNode * m_point;
    RootNode *m_rootNode;
    LabelNode *m_label;

    char m_spType;
    float m_size;

    LabelsItem::label_t m_labelType;

    short m_trixel; //Trixel to which this object belongs. Used only in stars. By default -1 for all
    //other objects that are not indexed by SkyMesh

    /**
     * @short isTextureRegenerated true if texture in m_point has to be regenerated
     */
    bool isTextureRegenerated;

    QPointF pos;
};

#endif

