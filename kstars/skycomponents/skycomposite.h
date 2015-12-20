/***************************************************************************
                          skycomposite.h  -  K Desktop Planetarium
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

#ifndef SKYCOMPOSITE_H
#define SKYCOMPOSITE_H

#include <QList>

#include "skycomponent.h"

class KSNumbers;

/**
	*@class SkyComposite
	*SkyComposite is a kind of container class for SkyComponent 
	*objects.  The SkyComposite is responsible for distributing calls 
	*to functions like draw() and update() to its children,
	*which can be SkyComponents or other SkyComposites with their 
	*own children.  This is based on the "composite/component"
	*design pattern.
	*
	*Performance tuning: Sometimes it will be better to override a 
	*virtual function and do the work in the composite instead of 
	*delegating the request to all sub components.
	*
	*@author Thomas Kabelmann
	*@version 0.1
	*/
class SkyComposite : public SkyComponent
{
public:
    /** @short Constructor
     * @p parent pointer to the parent SkyComponent
     */
    explicit SkyComposite( SkyComposite *parent );

    /** *@short Destructor */
    virtual ~SkyComposite();

    /** @short Delegate draw requests to all sub components
     * @p psky Reference to the QPainter on which to paint
     */
    virtual void draw( SkyPainter *skyp );

    /** @short Delegate update-position requests to all sub components
     *
     * This function usually just updates the Horizontal (Azimuth/Altitude)
     * coordinates.  However, the precession and nutation must also be 
     * recomputed periodically.  Requests to do so are sent through the
     * doPrecess parameter.
     * @p num Pointer to the KSNumbers object
     * @sa updatePlanets()
     * @sa updateMoons()
     * @note By default, the num parameter is NULL, indicating that 
     * Precession/Nutation computation should be skipped; this computation 
     * is only occasionally required.
     */
    virtual void update( KSNumbers *num=0 );

    /** @short Add a new sub component to the composite
     * @p comp Pointer to the SkyComponent to be added
     * @p priority A priority ordering for various operations on the list of all sky components (notably objectNearest())
     */
    void addComponent(SkyComponent *comp, int priority = 1024);

    /** @short Remove a sub component from the composite
     * @p comp Pointer to the SkyComponent to be removed.
     */
    void removeComponent(SkyComponent * const comp);

    /** @short Search the children of this SkyComposite for
     * a SkyObject whose name matches the argument.
     *
     * The objects' primary, secondary and long-form names will 
     * all be checked for a match.
     * @p name the name to be matched
     * @return a pointer to the SkyObject whose name matches
     * the argument, or a NULL pointer if no match was found.
     */
    virtual SkyObject* findByName( const QString &name );

    /** @short Identify the nearest SkyObject to the given SkyPoint,
     * among the children of this SkyComposite
     * @p p pointer to the SkyPoint around which to search.
     * @p maxrad reference to current search radius 
     * @return a pointer to the nearest SkyObject
     */
    virtual SkyObject* objectNearest( SkyPoint *p, double &maxrad );

    QList<SkyComponent*> components() { return m_Components.values(); }

    QMap<int, SkyComponent*>& componentsWithPriorities() { return m_Components; }

private:
    QMap<int, SkyComponent*> m_Components;
};

#endif
