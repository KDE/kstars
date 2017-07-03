/** *************************************************************************
                          skynode.h  -  K Desktop Planetarium
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

#pragma once

#include "skymaplite.h"
#include "projections/projector.h"
#include "../skyopacitynode.h"

#include <QSGTransformNode>

class Projector;
class SkyMapLite;

/**
 * @class SkyNode
 *
 * A QSGTransformNode derived class that has to be subclassed by node containers like PlanetNode and
 * PointSourceNode. SkyObject * that is passed as parameter to constructor is used in subclasses
 * to calculate new coordinates in update(). Subclasses have to implement hide() so that each of
 * their child nodes can be hidden.
 *
 *@short Provides virtual functions for update of coordinates and nodes hiding
 *@author Artem Fedoskin
 *@version 1.0
 */

class SkyNode : public QSGTransformNode
{
  public:
    /**
     * @brief Constructor
     * @param skyObject that is represented on the SkyMapLIte
     */
    SkyNode(SkyObject *skyObject);
    SkyNode();
    /**
     * @short short function that returns pointer to the current projector
     * @return pointer to current projector of SkyMapLite
     */

    /// All children nodes allocated on heap are deleted when parent is destroyed
    virtual ~SkyNode() {}

    inline const Projector *projector() { return SkyMapLite::Instance()->projector(); }

    /**
     * @short short function to access SkyMapLite
     * @return pointer to instance of SkyMapLite class
     */
    inline SkyMapLite *map() const { return SkyMapLite::Instance(); }

    /**
     * @short updates coordinate of the object on SkyMapLite
     */
    virtual void update() {}

    /**
     * @short sets m_drawLabel to true if it is needed to be drawn and calls update()
     * @param drawLabel true of label has to be drawn
     */
    void update(bool drawLabel);

    inline void addChildNode(QSGNode *node) { m_opacity->appendChildNode(node); }

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
    inline bool visible() { return m_opacity->visible(); }

    /**
     * @short returns SkyObject associated with this SkyNode
     * @return pointer to the object of type SkyObject
     */
    SkyObject *skyObject() const { return m_skyObject; }
    SkyOpacityNode *m_opacity;

  protected:
    SkyObject *m_skyObject;
    bool m_drawLabel;
    int m_hideCount;
};
