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

#include <QSGOpacityNode>
#include "labelsitem.h"

class SkyComponent;
class SkyMapLite;
class QQuickItem;
class RootNode;
class SkyNode;

/** @class SkyItem
 *
 *This is an interface for implementing SkyItems that are used to display SkyComponent
 *derived objects on the SkyMapLite.
 *
 *@short A base class that is used for displaying SkyComponents on SkyMapLite.
 *@author Artem Fedoskin
 *@version 1.0
 */

class SkyItem : public QSGOpacityNode {

public:
   /**
    *Constructor, add SkyItem to parent in a node tree
    *
    * @param parent a pointer to SkyItem's parent node
    */

    explicit SkyItem(LabelsItem::label_t labelType, RootNode *rootNode = 0);
    virtual ~SkyItem();

    virtual void update() =0;

    void hide();
    void show();

    inline RootNode *rootNode() { return m_rootNode; }

    inline LabelsItem::label_t labelType() { return m_labelType; }

    bool visible();

private:
    RootNode *m_rootNode;
    QVector<SkyNode *>m_skyNodes;

    //This node holds all labels that belongs to this SkyItem. See LabelsItem
    LabelTypeNode *m_labels;
    LabelsItem::label_t m_labelType;
};

#endif
