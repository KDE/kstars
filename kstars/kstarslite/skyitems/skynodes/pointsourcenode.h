/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "skynode.h"
#include "../labelsitem.h"

class LabelNode;
class PlanetItemNode;
class PointNode;
class RootNode;
class SkyMapLite;

/**
 * @class PointSourceNode
 *
 * A SkyNode derived class used for displaying PointNode with coordinates provided by SkyObject.
 *
 * @short A SkyNode derived class that represents stars and objects that are drawn as stars
 * @author Artem Fedoskin
 * @version 1.0
 */
class PointSourceNode : public SkyNode
{
  public:
    /**
     * @short Constructor
     * @param skyObject pointer to SkyObject that has to be displayed on SkyMapLite
     * @param parentNode pointer to the top parent node, which holds texture cache
     * @param labelType label type of PointNode
     * @param spType spectral class of PointNode
     * @param size initial size of PointNode
     * @param trixel trixelID, with which this node is indexed
     */
    PointSourceNode(SkyObject *skyObject, RootNode *parentNode,
                    LabelsItem::label_t labelType = LabelsItem::label_t::STAR_LABEL,
                    char spType = 'A', float size = 1, short trixel = -1);
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

    /** @short update updates coordinates of this node based on the visibility of its SkyObject */
    virtual void update() override;

    /**
     * @short hides this node and its label. m_point is hided without explicitly doing this because
     * it is a child node of m_opacity inherited from SkyNode
     */
    virtual void hide() override;

  private:
    PointNode *m_point { nullptr };
    RootNode *m_rootNode { nullptr };
    LabelNode *m_label { nullptr };
    char m_spType { 0 };
    float m_size { 0 };
    LabelsItem::label_t m_labelType { LabelsItem::NO_LABEL };
    /**
     * Trixel to which this object belongs. Used only in stars. By default -1 for
     * all other objects that are not indexed by SkyMesh
     */
    short m_trixel { 0 };
    QPointF pos;
};
