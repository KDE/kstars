/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "skynode.h"
#include "../labelsitem.h"

class PlanetItemNode;
class SkyMapLite;
class PointNode;
class LabelNode;
class QSGSimpleTextureNode;
class DSOSymbolNode;

/**
 * @class DeepSkyNode
 *
 *  @short A SkyNode derived class used for displaying DeepSkyObjects.
 *
 *  Keep in mind that DSO symbol is handled by DSOSymbolNode that has different parent from this node
 *  but DeepSkyNode calls update routines of DSOSymbolNode.
 *  @author Artem Fedoskin
 *  @version 1.0
 */

class RootNode;
class DeepSkyObject;

class DeepSkyNode : public SkyNode
{
  public:
    /**
     * @short Constructor.
     * @param skyObject - DSOs that is represented by this node
     * @param symbol - DSOSymbolNode of this DSO
     * @param labelType - type of label
     * @param trixel - trixelID, with which this node is indexed
     */
    DeepSkyNode(DeepSkyObject *skyObject, DSOSymbolNode *symbol, LabelsItem::label_t labelType, short trixel = -1);

    /** @short Destructor. Call delete routines of label */
    virtual ~DeepSkyNode();

    /**
     * @short changePos changes the position of this node and rotate it according to m_angle
     * @param pos new position
     */
    virtual void changePos(QPointF pos) override;

    /**
     * @short Update position and visibility of this node
     * @param drawImage - true if image (if exists) should be drawn
     * @param drawLabel - true if label should be drawn
     * @param pos - new position of the object. If default parameter is passed, the visibility and
     * position of node is calculated.
     * There is one case when we pass this parameter - in DeepSkyItem::updateDeepSkyNode() when
     * we check whether DeepSkyObject is visible or no and instantiate it accordingly. There is no
     * need to calculate the position again and we pass it as a parameter.
     */
    void update(bool drawImage, bool drawLabel, QPointF pos = QPointF(-1, -1));

    virtual void hide() override;

    /**
     * @short sets color of DSO symbol and label
     * To not increase the code for symbols we just recreate the symbol painted with desired color
     * @param color the color to be set
     * @param symbolTrixel the TrixelNode to which symbol node should be appended
     */
    void setColor(QColor color, TrixelNode *symbolTrixel);

    DeepSkyObject *dsObject() { return m_dso; }
    DSOSymbolNode *symbol() { return m_symbol; }

  private:
    QSGSimpleTextureNode *m_objImg { nullptr };
    /// Trixel to which this object belongs. Used only in stars. By default -1 for all
    Trixel m_trixel { 0 };

    LabelNode *m_label { nullptr };
    LabelsItem::label_t m_labelType { LabelsItem::NO_LABEL };

    DeepSkyObject *m_dso { nullptr };
    DSOSymbolNode *m_symbol { nullptr };
    float m_angle { 0 };
    QPointF pos;
};
