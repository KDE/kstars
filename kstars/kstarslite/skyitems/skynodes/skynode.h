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
#ifndef SKYNODE_H_
#define SKYNODE_H_
#include <QSGTransformNode>
#include "skymaplite.h"
#include "projections/projector.h"

class Projector;
class SkyMapLite;

/** @class SkyNode
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

class SkyNode : public QSGTransformNode {
public:
    /**
     * @brief Constructor
     * @param skyObject that is represented on the SkyMapLIte
     */
    SkyNode(SkyObject * skyObject);
    /** @short short function that returns pointer to the current projector
     *  @return pointer to current projector of SkyMapLite
     */

    // All children nodes allocated on heap are deleted automatically
    //virtual ~SkyNode() { }

    inline const Projector* projector() { return SkyMapLite::Instance()->projector(); }

    /** @short short function to access SkyMapLite
     *  @return pointer to instance of SkyMapLite class
     */
    inline const SkyMapLite* map() { return SkyMapLite::Instance(); }

    /**
     * @short updates coordinate of the object on SkyMapLite
     */
    virtual void update() =0;
    /**
     * @short hides all child nodes
     */
    virtual void hide() =0;

    /**
     * @short changePos changes the position of SkyNode on SkyMapLite
     * @param pos new position
     */
    virtual void changePos(QPointF pos) =0;

    /**
     * @short returns SkyObject associated with this SkyNode
     * @return pointer to the object of type SkyObject
     */
    SkyObject * getSkyObject() const { return m_skyObject; }
protected:
    SkyObject * m_skyObject;
private:
    //SkyMapLite * m_skyMapLite;
    //const Projector * m_proj;
};


#endif
