/***************************************************************************
                    starblocklist.h  -  K Desktop Planetarium
                             -------------------
    begin                : Mon 9 Jun 2008
    copyright            : (C) 2008 by Akarsh Simha
    email                : akarshsimha@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef STARBLOCKLIST_H
#define STARBLOCKLIST_H

#include "starblock.h"

class StarBlock;

/**
 *@class StarBlockList
 *Maintains a list of StarBlocks that contain the stars lying in a single trixel.
 *Takes care of the dynamic loading of stars
 *@author Akarsh Simha
 *@version 0.1
 */

class StarBlockList {
 public:

    /**
     *Constructor.
     */
    StarBlockList();

    /**
     *Destructor
     */
    ~StarBlockList();

    /**
     *@short  Drops the StarBlock with the given pointer from the list
     *@param  Pointer to the StarBlock to remove
     */
    void releaseBlock( StarBlock *block );

 private:
    QList < StarBlock *> list;
    quint32 readOffset;
};

#endif
