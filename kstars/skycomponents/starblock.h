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
#include "starblocklist.h"

#include <QVector>

class StarObject;
class StarBlockList;

/**
 *@class StarBlock
 *Holds a block of stars and various peripheral variables to mark its place in data structures
 *
 *@author  Akarsh Simha
 *@version 1.0
 */

class StarBlock {

 public:

    /**
     * Constructor
     * Initializes values of various parameters and creates a predefined number of stars
     */
    StarBlock();

    /**
     * Constructor
     * Initializes values of various parameters and creates nstars number of stars
     *@param nstars   Number of stars to hold in this StarBlock
     */
    StarBlock( int nstars );

    /**
     * Constructor
     * Initializes values of various parameters and links this StarBlock to its neighbours
     *@param  _prev  Pointer to the previous StarBlock in the linked list
     *@param  _next  Pointer to the next StarBlock in the linked list
     */
    StarBlock( StarBlock *_prev, StarBlock *_next );

    /**
     * Destructor
     * Deletes any stored stars
     */
    ~StarBlock();


    StarBlockList *parent;
    StarBlock *prev;
    StarBlock *next;
    quint32 drawID;
    int starsRead;
    QVector< StarObject *> stars;
    float faintMag;
    float brightMag;

 private:

    /**
     *@short Allocate Space for nstars stars and put the pointers in the stars vector
     *
     *@param nstars  Number of stars to allocate space for
     *@return Number of StarObjects successfully allocated
     */
    int allocStars( int nstars );

    /**
     *@short  Do the real construction work
     *
     *@param  _prev  Pointer to previous StarBlock in the linked list
     *@param  _next  Pointer to next StarBlock in the linked list
     */
    void init( StarBlock *_prev, StarBlock *_next );

    static StarObject *plainStarTemplate;
    static int refCount;
};

#endif
