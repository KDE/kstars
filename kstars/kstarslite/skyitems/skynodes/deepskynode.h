/** *************************************************************************
                          deepskynode.h  -  K Desktop Planetarium
                             -------------------
    begin                : 20/05/2016
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
#ifndef DEEPSKYNODE_H_
#define DEEPSKYNODE_H_
#include "skynode.h"
#include "../labelsitem.h"

class PlanetItemNode;
class SkyMapLite;
class PointNode;
class LabelNode;
class QSGSimpleTextureNode;
class DSOSymbolNode;

/** @class DeepSkyNode
 *
 * A SkyNode derived class used for displaying PointNode with coordinates provided by SkyObject.
 *
 *@short A SkyNode derived class that represents stars and objects that are drawn as stars
 *@author Artem Fedoskin
 *@version 1.0
 */

class RootNode;

class DeepSkyNode : public SkyNode  {
public:
    /**
     * @short Constructor
     * @param skyObject pointer to SkyObject that has to be displayed on SkyMapLite
     * @param parentNode pointer to the top parent node, which holds texture cache
     * @param spType spectral class of PointNode
     * @param size initial size of PointNode
     */
    DeepSkyNode(DeepSkyObject *skyObject, DSOSymbolNode *symbol, Trixel trixel, LabelsItem::label_t labelType);

    /**
     * @short changePos changes the position m_point
     * @param pos new position
     */
    void changePos(QPointF pos);

    void update(bool drawImage, bool drawLabel);
    virtual void hide() override;

    DeepSkyObject *dsObject() { return m_dso; }
private:
    QSGSimpleTextureNode *m_objImg;
    Trixel m_trixel; //Trixel to which this object belongs. Used only in stars. By default -1 for all

    LabelNode *m_label;
    LabelsItem::label_t m_labelType;

    DeepSkyObject *m_dso;
    DSOSymbolNode *m_symbol;
    float m_angle;
    QPointF pos;
};

#endif

