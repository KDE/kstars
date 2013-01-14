/***************************************************************************
                          listcomponent.h  -  K Desktop Planetarium
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

#ifndef LISTCOMPONENT_H
#define LISTCOMPONENT_H

#include <QList>

#include "skycomponent.h"

class SkyComposite;
class SkyMap;

/**
 *@class ListComponent
 *An abstract parent class, to be inherited by SkyComponents that store a QList
 *of SkyObjects.
 *
 *@author Jason Harris
 *@version 0.1
 */
class ListComponent : public SkyComponent
{
public:
    explicit ListComponent( SkyComposite *parent );

    virtual ~ListComponent();

    /**@short Update the sky positions of this component.
     *
     * This function usually just updates the Horizontal (Azimuth/Altitude)
     * coordinates of the objects in this component.  If the KSNumbers* 
     * argument is not NULL, this function also recomputes precession and
     * nutation for the date in KSNumbers.
     * @p num Pointer to the KSNumbers object
     * @note By default, the num parameter is NULL, indicating that 
     * Precession/Nutation computation should be skipped; this computation 
     * is only occasionally required.
     */
    virtual void update( KSNumbers *num=0 );

    virtual SkyObject* findByName( const QString &name );
    virtual SkyObject* objectNearest( SkyPoint *p, double &maxrad );

    void clear();

    const QList<SkyObject*>& objectList() const { return m_ObjectList; }

protected:
    QList<SkyObject*> m_ObjectList;
};


#endif
