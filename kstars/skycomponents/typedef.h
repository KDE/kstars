/***************************************************************************
                          typedef.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2007-07-03
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

/* It is a bit cheesy/crappy to have somewhat unrelated typedefs gathered in a
 * single typedef.h file like this but it has been incredibly useful For
 * example, I could convert the list structures below from QList to QVector by
 * simply making a single change in this file.  Also, typedefs can take people
 * by surprise so by putting most of them all in one place I hope to remove
 * some of the surprise.
 *
 * -- James B. Bowlin
 */

#ifndef TYPEDEF_H_
#define TYPEDEF_H_

#include <QHash>
#include <QVector>

class SkyPoint;
class LineList;
class StarObject;
class StarBlock;
class SkyObject;

typedef quint32                        DrawID;
typedef quint32                        UpdateID;
typedef unsigned int                   Trixel;
typedef unsigned short                 BufNum;

typedef QVector< SkyPoint*>            SkyList;
typedef QHash< Trixel, bool>           IndexHash;
typedef QHash< Trixel, bool>           SkyRegion;
typedef QList< StarObject*>            StarList;
typedef QVector< StarList*>            StarIndex;
typedef QVector< LineList*>            LineListList;
typedef QHash< Trixel, LineListList*>  LineListHash;  // Wanted LineListIndex, but that is used by a class
typedef QList< SkyObject *>            SkyObjectList;

#endif
