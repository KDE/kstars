/** *************************************************************************
                          labelnode.h  -  K Desktop Planetarium
                             -------------------
    begin                : 09/06/2016
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
     * @param skyObject pointer to SkyObject that has to be displayed on SkyMapLite
     * @param parentNode pointer to the top parent node, which holds texture cache
     * @param spType spectral class of PointNode
     * @param size initial size of PointNode
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
