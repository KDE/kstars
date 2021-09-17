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
class QSGSimpleTextureNode;
class SkyLabeler;
class RootNode;

/**
 * @class LabelNode
 *
 * @short A SkyNode derived class used for displaying labels
 * @author Artem Fedoskin
 * @version 1.0
 */

class LabelNode : public SkyNode
{
  public:
    /**
     * @short Constructor. Use name of skyObject as a text
     * @param skyObject - target object, for which this label is created.
     * @param type - type of label (corresponds to type of SkyObject)
     */
    LabelNode(SkyObject *skyObject, LabelsItem::label_t type);

    /**
     * @short Constructor. Use string parameter name as a text
     * @param name - text of label
     * @param type - type of label (corresponds to type of SkyObject)
     */
    LabelNode(QString name, LabelsItem::label_t type);

    /**
     * @short Destructor.
     */
    virtual ~LabelNode();

    /**
     * @short Convenience function to not to repeat the same code in 2 constructors. Set parameters of label
     * based on its type
     */
    void initialize();

    /**
     * @short Changes position of the label
     * @param pos - new position
     */
    virtual void changePos(QPointF pos) override;

    inline QString name() { return m_name; }

    inline LabelsItem::label_t labelType() { return m_labelType; }

    /**
     * @short Create texture from label's name
     * @param color - color of the label
     */
    void createTexture(QColor color = QColor());

    /**
     * @return true if the size of text depends on zoom
     */
    inline bool zoomFont() { return m_zoomFont; }

    /**
     * @short set the position of label with the given offset from SkyObject's position and
     * makes the label visible if it was hidden
     * @warning Keep mind that to update labels position, you should first set it with setLabelPos()
     * and then call update()
     * @param pos position of label
     */
    void setLabelPos(QPointF pos);

    /**
     * @short Update position of label according to labelPos and recreate texture if label's size
     * depends on zoom level
     */
    virtual void update() override;

    QPointF labelPos;

  private:
    QString m_name;
    QSGSimpleTextureNode *m_textTexture;
    QSize m_textSize;

    LabelsItem::label_t m_labelType;
    int m_fontSize;
    bool m_zoomFont;
    QString m_schemeColor;
    QColor m_color;
};
