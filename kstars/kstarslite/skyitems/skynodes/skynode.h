/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "skymaplite.h"

#include <QSGTransformNode>

class Projector;
class SkyOpacityNode;

/**
 * @class SkyNode
 * @short Provides virtual functions for update of coordinates and nodes hiding
 *
 * A QSGTransformNode derived class that has to be subclassed by node containers like PlanetNode and
 * PointSourceNode. SkyObject * that is passed as parameter to constructor is used in subclasses
 * to calculate new coordinates in update(). Subclasses have to implement hide() so that each of
 * their child nodes can be hidden.
 *
 * @author Artem Fedoskin
 * @version 1.0
 */
class SkyNode : public QSGTransformNode
{
  public:
    /**
     * @brief Constructor
     * @param skyObject that is represented on the SkyMapLIte
     */
    explicit SkyNode(SkyObject *skyObject);
    SkyNode();

    virtual ~SkyNode() {}

    inline const Projector *projector() { return SkyMapLite::Instance()->projector(); }

    /**
     * @short short function to access SkyMapLite
     * @return pointer to instance of SkyMapLite class
     */
    inline SkyMapLite *map() const { return SkyMapLite::Instance(); }

    /** Updates coordinate of the object on SkyMapLite */
    virtual void update() {}

    /**
     * @short sets m_drawLabel to true if it is needed to be drawn and calls update()
     * @param drawLabel true of label has to be drawn
     */
    void update(bool drawLabel);

    void addChildNode(QSGNode *node);

    /**
     * @short hides all child nodes (sets opacity of m_opacity to 0)
     */
    virtual void hide();

    /**
     * @short shows all child nodes (sets opacity of m_opacity to 1)
     */
    virtual void show();

    inline int hideCount() { return m_hideCount; }

    /**
     * @short changes the position of SkyNode on SkyMapLite. Has to be overridden by the classes
     * that support moving
     * @param pos new position
     */
    virtual void changePos(QPointF pos) { Q_UNUSED(pos); }

    /**
     * @return true if object is visible (m_opacity->opacity() != 0) else returns false
     */
    bool visible();

    /**
     * @short returns SkyObject associated with this SkyNode
     * @return pointer to the object of type SkyObject
     */
    SkyObject *skyObject() const { return m_skyObject; }

    SkyOpacityNode *m_opacity { nullptr };

  protected:
    SkyObject *m_skyObject { nullptr };
    bool m_drawLabel { false };
    int m_hideCount { 0 };
};
