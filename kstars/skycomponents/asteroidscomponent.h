/***************************************************************************
                          asteroidscomponent.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/30/08
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

#ifndef ASTEROIDSCOMPONENT_H
#define ASTEROIDSCOMPONENT_H

#include "solarsystemlistcomponent.h"
#include <QList>
#include <ksparser.h>
#include "typedef.h"

/**@class AsteroidsComponent
 * Represents the asteroids on the sky map.
 *
 * @author Thomas Kabelmann
 * @version 0.1
 */
class AsteroidsComponent: public SolarSystemListComponent
{
public:
    /**@short Default constructor.
     * @p parent pointer to the parent SolarSystemComposite
     */
    AsteroidsComponent(SolarSystemComposite *parent);

    virtual ~AsteroidsComponent();
    virtual void draw( SkyPainter *skyp );
    virtual bool selected();
    virtual SkyObject* objectNearest( SkyPoint *p, double &maxrad );
    void updateDataFile();
    QString ans();
private:
    void loadData();
};

#endif
