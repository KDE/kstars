/***************************************************************************
                          equator.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2007-08-09
    copyright            : (C) 2007 by James B. Bowlin
    email                : bowlin@mindspring.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef EQUATOR_H
#define EQUATOR_H

#include "noprecessindex.h"
#include "linelistlabel.h"


/**
 * @class Equator
 * Represents the equator on the sky map.
 *
 * @author James B. Bowlin
 * @version 0.1
 */
class Equator : public NoPrecessIndex
{
public:

    /* @short Constructor
     * @p parent pointer to the parent SkyComposite object
     * name is the name of the subclass
     */
    explicit Equator( SkyComposite *parent );

    virtual bool selected();
    virtual void draw( SkyPainter *skyp );
    virtual void drawCompassLabels();
    virtual LineListLabel* label() {return &m_label;};

protected:
    virtual void preDraw( SkyPainter *skyp );

private:
    LineListLabel m_label;
};

#endif
