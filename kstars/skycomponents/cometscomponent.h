/***************************************************************************
                          cometscomponent.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/24/09
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

#ifndef COMETSCOMPONENT_H
#define COMETSCOMPONENT_H

class SkyLabeler;

#include "solarsystemlistcomponent.h"
#include <QList>
#include <ksparser.h>

/**@class CometsComponent
 * This class encapsulates the Comets
 *
 * @author Jason Harris
 * @version 0.1
 */
class CometsComponent : public SolarSystemListComponent
{
public:
    /**@short Default constructor.
     * @p parent pointer to the parent SolarSystemComposite
     */
    CometsComponent(SolarSystemComposite *parent);

    virtual ~CometsComponent();
    virtual bool selected();
    virtual void draw( SkyPainter *skyp );
    void updateDataFile();
private:
    void loadData();
};

#endif
