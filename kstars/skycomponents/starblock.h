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
struct starData;
struct deepStarData;

/**
 *@class StarBlock
 *Holds a block of stars and various peripheral variables to mark its place in data structures
 *
 *@author  Akarsh Simha
 *@version 1.0
 */
class StarBlock
{
public:
    /** Constructor
     *  Initializes values of various parameters and creates nstars number of stars
     *  @param nstars   Number of stars to hold in this StarBlock
     */
    explicit StarBlock( int nstars = 100 );

    /**                                                                                 
     * Destructor                                                                       
     * Deletes all stored stars                                                         
     */
    ~StarBlock();

    /** @short Initialize another star with data. 
     *
     *  FIXME: StarObject::init doesn't reset object name(s). It
     *  shouldn't be issue since stars which are swapped in/out do not
     *  have names. 
     *
     *@param  data    data to initialize star with.
     *@return pointer to star initialized with data. NULL if block is full.
     */
    StarObject* addStar(const starData& data);
    StarObject* addStar(const deepStarData& data);

    /**
     *@short Returns true if the StarBlock is full
     *
     *@return true if full, false if not full
     */
    inline bool isFull() const { return nStars == size(); }

    /**
     *@short  Return the capacity of this StarBlock
     *
     *This is different from nStars. While nStars indicates the number of stars that this StarBlock
     *actually holds, this method returns the number of stars for which we have allocated memory.
     *Thus, this method should return a number >= nStars.
     *
     *@return The number of stars that this StarBlock can hold
     */
    inline int size() const { return stars.size(); }

    /**
     *@short  Return the i-th star in this StarBlock
     *
     *@param  Index of StarBlock to return
     *@return A pointer to the i-th StarObject
     */
    inline StarObject *star( int i ) { return &stars[i]; }

    // These methods are there because we might want to make faintMag and brightMag private at some point
    /**
     *@short  Return the magnitude of the brightest star in this StarBlock
     *
     *@return Magnitude of the brightest star
     */
    inline float getBrightMag() const { return brightMag; }

    /**
     *@short  Return the magnitude of the faintest star in this StarBlock
     *
     *@return Magnitude of the faintest star
     */
    inline float getFaintMag() const { return faintMag; }

    /**
     *@short  Return the number of stars currently filled in this StarBlock
     *@return Number of stars filled in this StarBlock
     */
    inline int getStarCount() const { return nStars; }

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
    // Disallow copying and assignment. Just in case.
    StarBlock(const StarBlock&);
    StarBlock& operator = (const StarBlock&);

    /** Number of initialized stars in StarBlock. */
    int nStars;
    /** Array of stars. */
    QVector<StarObject> stars;
};

#endif
