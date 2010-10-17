/***************************************************************************
                          linelist.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2007-07-06
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

#ifndef LINELIST_H
#define LINELIST_H

#include <QList>

#include "typedef.h"

class SkyPoint;
class KSNumbers;

/* @class LineList
 * A simple data container used by LineListIndex.  It contains a list of
 * SkyPoints and integer drawID, updateID and updateNumID. 
 *
 * @author James B. Bowlin
 * @version 0.2
*/
class LineList
{
public:
    LineList() : drawID(0), updateID(0), updateNumID(0) {}

    /* A global drawID (in SkyMesh) is updated at the start of each draw
     * cycle.  Since an extended object is often covered by more than one
     * trixel, the drawID is used to make sure each object gets drawn at
     * most once per draw cycle.  It is public because it is both set and
     * read by the LineListIndex class.
     */
    DrawID   drawID;
    UpdateID updateID;
    UpdateID updateNumID;

    /* @short return the list of points for iterating or appending
     * (or whatever).
     */
    SkyList* points() { return &pointList; }
    SkyPoint* at( int i ) { return pointList.at( i ); }
    void append( SkyPoint* p ) { pointList.append( p ); }
private:
    SkyList pointList;

};

#endif
