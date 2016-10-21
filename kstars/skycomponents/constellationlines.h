/***************************************************************************
               constellationlines.h  -  K Desktop Planetarium
                             -------------------
    begin                : 25 Oct. 2005
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

#ifndef CONSTELLATIONLINES_H
#define CONSTELLATIONLINES_H

#include <QHash>

#include "linelistindex.h"
#include "ksnumbers.h"

class CultureList;

/**
*@class ConstellationLines
*Collection of lines making the 88 constellations

*@author Jason Harris
*@version 0.1
*/

class ConstellationLines : public LineListIndex
{

public:
    /** @short Constructor
     * @p parent Pointer to the parent SkyComposite object
     *
     * Constellation lines data is read from clines.dat.
     * Each line in the file contains a command character ("M" means move to 
     * this position without drawing a line, "D" means draw a line from 
     * the previous position to this one), followed by the genetive name of 
     * a star, which marks the position of the constellation node.
     */
    ConstellationLines( SkyComposite *parent, CultureList* cultures );

    void reindex( KSNumbers *num );

    virtual bool selected();

protected:
    const IndexHash& getIndexHash(LineList* lineList );

    /** @short we need to override the update routine because stars are
     * updated differently from mere SkyPoints.
     */
    virtual void JITupdate( LineList* lineList );

    /** @short Set the QColor and QPen for drawing. */
    virtual void preDraw( SkyPainter* skyp );

private:

    KSNumbers m_reindexNum;
    double    m_reindexInterval;

};


#endif
