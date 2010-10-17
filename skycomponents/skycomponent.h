/***************************************************************************
                          skycomponent.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/07/08
    copyright            : (C) 2005 by Thomas Kabelmann
    email                : thomas.kabelmann@gmx.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef SKYCOMPONENT_H
#define SKYCOMPONENT_H


#include <kdebug.h>
#include "typedef.h"

class QPainter;
class QString;

class KSNumbers;
class SkyObject;
class SkyPoint;
class SkyComposite;

/**
 * @class SkyComponent
 * SkyComponent represents an object on the sky map. This may be a
 * star, a planet or an imaginary line like the equator.
 * The SkyComponent uses the composite design pattern.  So if a new
 * object type is needed, it has to be derived from this class and
 * must be added into the hierarchy of components.
 * ((TODO_DOX: Add outline of Components hierarchy))
 *
 * @author Thomas Kabelmann
 */
class SkyComponent
{
public:
    /**@short Constructor
     * @p parent pointer to the parent SkyComposite
     */
    explicit SkyComponent( SkyComposite *parent );

    /** @short Destructor */
    virtual ~SkyComponent();

    /**@short Draw the object on the SkyMap
     * @p psky Reference to the QPainter on which to paint
     */
    virtual void draw( QPainter& psky ) = 0;

    /**@short Draw trails for objects. */
    virtual void drawTrails( QPainter & );

    /**
     * @short Update the sky position(s) of this component.
     *
     * This function usually just updates the Horizontal (Azimuth/Altitude)
     * coordinates of its member object(s).  However, the precession and
     * nutation must also be recomputed periodically.
     * @p num Pointer to the KSNumbers object
     * @sa SingleComponent::update()
     * @sa ListComponent::update()
     * @sa ConstellationBoundaryComponent::update()
     */
    virtual void update( KSNumbers* ) {}
    virtual void updatePlanets( KSNumbers * ) {}
    virtual void updateMoons( KSNumbers * ) {}

    /**@return true if component is to be drawn on the map. */
    virtual bool selected() { return true; }

    /**@return Parent of component. If there is no parent returns NULL. */
    SkyComposite* parent() { return m_parent; }

    /**
     * @short Search the children of this SkyComponent for
     * a SkyObject whose name matches the argument
     * @p name the name to be matched
     * @return a pointer to the SkyObject whose name matches
     * the argument, or a NULL pointer if no match was found.
     * @note This function simply returns the NULL pointer; it
     * is reimplemented in various sub-classes
     */
    virtual SkyObject* findByName( const QString &name );

    /**
     * @short Searches the region(s) and appends the SkyObjects found to the list of sky objects
     *
     * Look for a SkyObject that is in one of the regions 
     * If found, then append to the list of sky objects
     * @p list list of SkyObject to which matching list has to be appended to
     * @p region defines the regions in which the search for SkyObject should be done within
     * @return void
     * @note This function simply returns; it is
     * reimplemented in various sub-classes.
     */
    virtual void objectsInArea( QList<SkyObject*>& list, const SkyRegion& region );

    /**
     * @short Find the SkyObject nearest the given SkyPoint
     *
     * Look for a SkyObject that is nearer to point p than maxrad.
     * If one is found, then maxrad is reset to the separation of the new nearest object.
     * @p p pointer to the SkyPoint to search around
     * @p maxrad reference to current search radius
     * @return a pointer to the nearest SkyObject
     * @note This function simply returns a NULL pointer; it is
     * reimplemented in various sub-classes.
     */
    virtual SkyObject* objectNearest( SkyPoint *p, double &maxrad );

    /**@short Emit signal about progress.
     *
     * @sa SkyMapComposite::emitProgressText
     */
    virtual void emitProgressText( const QString &message );

    inline QHash<int, QStringList>& objectNames() { return getObjectNames(); }

    inline QStringList& objectNames(int type) { return getObjectNames()[type]; }

protected:
    void removeFromNames(const SkyObject* obj);

private:
    /** */
    virtual QHash<int, QStringList>& getObjectNames();

    // Disallow copying and assignement
    SkyComponent(const SkyComponent&);
    SkyComponent& operator= (const SkyComponent&);

    /** Parent of sky component. */
    SkyComposite *m_parent;
};

#endif
