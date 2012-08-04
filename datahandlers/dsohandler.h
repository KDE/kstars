/***************************************************************************
                          DSOHandler.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 2012/03/08
    copyright            : (C) 2012 by Rishab Arora
    email                : ra.rishab@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef DSOHANDLER_H
#define DSOHANDLER_H

/* Some notes about the database. (skycomponents.db)
 * 1) The uid for Object Designation is the uid being used by objects in KStars
 *    hence, the uid is a qint64 i.e. a 64 bit signed integer. Coincidentaly,
 *    this is the max limit of an int in Sqlite3.
 *    
 */

class DSOHandler
{
};

#endif // DSOHANDLER_H
