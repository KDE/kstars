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
     * Destructor
     * Deletes any stored stars
     */
    ~StarBlock();

    /**
     *@short Make a copy of the given StarObject in this StarBlock
     *
     *This method uses memcpy() to copy the given StarObject into the next un-initialized
     *StarObject in this StarBlock. If the capacity of this StarBlock is exceeded, it 
     *returns false.
     *
     *@param  star  Pointer to a StarObject
     *@return true on success, false on failure
     */
    bool addStar( StarObject *star );

    /**
     *@short Returns true if the StarBlock is full
     *
     *@return true if full, false if not full
     */
    inline bool isFull() { return ( ( nStars == size() ) ? true : false ); }

    /**
     *@short  Return the capacity of this StarBlock
     *
     *This is different from nStars. While nStars indicates the number of stars that this StarBlock
     *actually holds, this method returns the number of stars for which we have allocated memory.
     *Thus, this method should return a number >= nStars.
     *
     *@return The number of stars that this StarBlock can hold
     */
    inline int size() { return stars.size(); }

    /**
     *@short  Return the i-th star in this StarBlock
     *
     *@param  Index of StarBlock to return
     *@return A pointer to the i-th StarObject
     */
    inline StarObject *star( int i ) { return stars.at( i ); }

    // These methods are there because we might want to make faintMag and brightMag private at some point
    /**
     *@short  Return the magnitude of the brightest star in this StarBlock
     *
     *@return Magnitude of the brightest star
     */
    inline float getBrightMag() { return brightMag; }

    /**
     *@short  Return the magnitude of the faintest star in this StarBlock
     *
     *@return Magnitude of the faintest star
     */
    inline float getFaintMag() { return faintMag; }

    /**
     *@short  Return the number of stars currently filled in this StarBlock
     *@return Number of stars filled in this StarBlock
     */
    inline int getStarCount() { return nStars; }

    /**
     *@short  Reset this StarBlock's data, for reuse of the StarBl
     */
    void reset();

    float faintMag;
    float brightMag;
    StarBlockList *parent;
    StarBlock *prev;
    StarBlock *next;
    quint32 drawID;

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
     */
    void init();

    int nStars;
    QVector< StarObject *> stars;

    static StarObject *plainStarTemplate;
    static int refCount;
};

#endif
