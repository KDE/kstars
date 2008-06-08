/***************************************************************************
                    starblock.h  -  K Desktop Planetarium
                             -------------------
    begin                : 8 Jun 2008
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

#ifndef STARBLOCK_H
#define STARBLOCK_H

#include "typedef.h"

class StarObject;

/**
 *@short   Holds a block of stars and various peripheral variables to mark its place in data structures
 *
 *@author  Akarsh Simha
 *@version 1.0
 */

typedef struct StarBlock {
    StarBlockList *parent;
    struct StarBlock *prev;
    struct StarBlock *next;
    long useID;
    StarObject *star;
    int nstars;
    float faintMag;
    float brightMag;
} StarBlock;

#endif


