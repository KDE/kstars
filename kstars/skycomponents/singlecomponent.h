/***************************************************************************
                          singlecomponent.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/10/01
    copyright            : (C) 2005 by Jason Harris
    email                : kstars@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef SINGLECOMPONENT_H
#define SINGLECOMPONENT_H

class SkyComposite;

#include "skycomponent.h"

/**
 *@class SingleComponent
 *An abstract parent class, to be inherited by SkyComponents that store a 
 *SkyObject.
 *
 *@author Jason Harris
 *@version 0.1
 */
class SingleComponent : public SkyComponent
{
public:
    SingleComponent( SkyComponent *parent, bool (*visibleMethod)() );

    virtual ~SingleComponent();

    /**@short Draw the object on the SkyMap
     * @p psky Reference to the QPainter on which to paint
     */
    virtual void draw( QPainter& /*psky*/ ) {}

    /**@short Update the sky position of this component.
     *
     * This function usually just updates the Horizontal (Azimuth/Altitude)
     * coordinates of the object in this component.  However, the precession
     * and nutation must also be recomputed periodically.  Requests to do
     * so are sent through the doPrecess parameter.
     * @p num Pointer to the KSNumbers object
     * @note By default, the num parameter is NULL, indicating that 
     * Precession/Nutation computation should be skipped; this computation 
     * is only occasionally required.
     */
    virtual void update( KSNumbers *num=0 );

    virtual SkyObject* findByName( const QString &name );

    virtual SkyObject* objectNearest( SkyPoint *p, double &maxrad );

    virtual SkyObject* first();
    virtual SkyObject* next();

    SkyObject* skyObject() { return m_StoredObject; }
    void setStoredObject( SkyObject *o ) { m_StoredObject = o; }

private:
    SkyComposite *Parent;
    SkyObject* m_StoredObject;
};

#endif
