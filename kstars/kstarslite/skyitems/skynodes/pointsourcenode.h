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
                    LabelsItem::label_t labelType = LabelsItem::label_t::STAR_LABEL, char spType = 'A', float size = 1);

    /** @short Get the width of a star of magnitude mag */
    float starWidth(float mag) const;

    /**
     * @short changePos changes the position m_point
     * @param pos new position
     */
    virtual void changePos(QPointF pos) override;

    virtual void update() override;
    virtual void hide() override;
private:
    PointNode * m_point;
    //TODO deal setter for this when stars will be introduced
    float m_sizeMagLim;
    RootNode *m_rootNode;

    LabelNode *m_label;
    LabelsItem::label_t m_labelType;

    QPointF pos;
};

#endif

