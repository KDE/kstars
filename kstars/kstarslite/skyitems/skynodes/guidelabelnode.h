/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "skynode.h"
#include "../labelsitem.h"

#include <QSGSimpleRectNode>

class PlanetItemNode;
class PointNode;
class QSGSimpleTextureNode;
class RootNode;
class SkyLabeler;
class SkyMapLite;

/**
 * @class GuideLabelNode
 * Currently this class is not used anywhere.
 *
 * @author Artem Fedoskin
 * @version 1.0
 */
class GuideLabelNode : public SkyNode
{
  public:
    /**
     * @short Constructor
     * @param name name of the guide label
     * @param type type of the label item
     */
    GuideLabelNode(QString name, LabelsItem::label_t type);

    /**
     * @short changePos changes the position m_point
     * @param pos new position
     */
    virtual void changePos(QPointF pos) override;

    /**
     * @short setLabelPos sets the position of label with the given offset from SkyObject's position and
     * makes the label visible if it was hidden
     * @param pos position of label
     * @param angle label angle
     */
    void setLabelPos(QPointF pos, float angle);

    virtual void update() override;

    inline const QString name() const { return m_name; }

    QPointF labelPos;
    qreal left { 0 };
    qreal right { 0 };
    qreal top { 0 };
    qreal bot { 0 };

  private:
    QSGSimpleTextureNode *m_textTexture { nullptr };
    QSize m_textSize { 0, 0 };
    float m_angle { 0 };
    QSGSimpleRectNode debugRect;
    const QString m_name;
    QPointF m_translatePos;
};
