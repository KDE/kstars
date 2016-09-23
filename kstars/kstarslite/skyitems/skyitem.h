/** *************************************************************************
                          skyitem.h  -  K Desktop Planetarium
                             -------------------
    begin                : 02/05/2016
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

#ifndef SKYITEM_H_
#define SKYITEM_H_

#include "skyopacitynode.h"
#include "labelsitem.h"

class SkyComponent;
class SkyMapLite;
class QQuickItem;
class RootNode;
class SkyNode;

/** @class SkyItem
 *
 *This is an interface for implementing SkyItems that represent SkyComponent derived objects on
 *the SkyMapLite. It is derived from QSGOpacityNode to make it possible to hide the whole node tree
 *by simply setting opacity to 0.
 *
 *@short A base class that is used for displaying SkyComponents on SkyMapLite.
 *@author Artem Fedoskin
 *@version 1.0
 */

class SkyItem : public SkyOpacityNode {

public:
   /**
    * Constructor, appends SkyItem to rootNode as a child in a node tree
    *
    * @param labelType type of label that corresponds to this item
    * @note see LabelsItem::label_t
    * @param parent a pointer to SkyItem's parent node
    */

    explicit SkyItem(LabelsItem::label_t labelType, RootNode *rootNode = 0);
    /**
     * @see PointSourceNode::~PointSourceNode()
     */
    virtual ~SkyItem();

    /**
     * @short updates the coordinates and visibility of child node. Similar to draw routine in
     * SkyComponent derived classes
     */

    virtual void update() =0;

    virtual void show() override;

    /**
     * @short hides this item and corresponding labels
     */
    virtual void hide() override;

    void hideLabels();

    /**
     * @return RootNode that is the parent of this SkyItem in a node tree
     */

    inline RootNode *rootNode() { return m_rootNode; }

    /**
     * @return label type of this SkyItem
     */

    inline LabelsItem::label_t labelType() { return m_labelType; }

private:
    RootNode *m_rootNode;
    QVector<SkyNode *>m_skyNodes;

    //This node holds all labels that belongs to this SkyItem. See LabelsItem
    LabelTypeNode *m_labels;
    LabelsItem::label_t m_labelType;
};

#endif
