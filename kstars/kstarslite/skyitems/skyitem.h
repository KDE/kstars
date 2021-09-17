/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "skyopacitynode.h"
#include "labelsitem.h"

class SkyComponent;
class SkyMapLite;
class QQuickItem;
class RootNode;
class SkyNode;

/**
 * @class SkyItem
 *
 * This is an interface for implementing SkyItems that represent SkyComponent derived objects on
 * the SkyMapLite. It is derived from QSGOpacityNode to make it possible to hide the whole node tree
 * by simply setting opacity to 0.
 *
 * @short A base class that is used for displaying SkyComponents on SkyMapLite.
 * @author Artem Fedoskin
 * @version 1.0
 */

class SkyItem : public SkyOpacityNode
{
  public:
    /**
     * Constructor, appends SkyItem to rootNode as a child in a node tree
     *
     * @param labelType type of label that corresponds to this item
     * @note see LabelsItem::label_t
     * @param parent a pointer to SkyItem's parent node
     */
    explicit SkyItem(LabelsItem::label_t labelType, RootNode *rootNode = nullptr);
    /** @see PointSourceNode::~PointSourceNode() */
    virtual ~SkyItem();

    /**
     * @short updates the coordinates and visibility of child node. Similar to draw routine in
     * SkyComponent derived classes
     */
    virtual void update() = 0;

    virtual void show() override;

    /** @short hides this item and corresponding labels */
    virtual void hide() override;

    void hideLabels();

    /** @return RootNode that is the parent of this SkyItem in a node tree */
    inline RootNode *rootNode() { return m_rootNode; }

    /** @return label type of this SkyItem */
    inline LabelsItem::label_t labelType() { return m_labelType; }

  private:
    RootNode *m_rootNode { nullptr };
    QVector<SkyNode *> m_skyNodes;
    LabelsItem::label_t m_labelType;
};
