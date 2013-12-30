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
#include "deepstarcomponent.h"
#include "typedef.h"

class StarBlock;
class DeepStarComponent;

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
    explicit StarBlockList( Trixel trixel );

    /**
     *Constructor for deep star catalogs.
     *@param trixel The trixel ID
     *@param parent Pointer to the parent DeepStarComponent
     */
    StarBlockList( Trixel trixel, DeepStarComponent *parent );

    /**
     *Destructor
     */
    ~StarBlockList();

    /**
     *@short Ensures that the list is loaded with stars to given magnitude limit
     *
     *@param Magnitude limit to load stars upto
     *@return true on success, false on failure (data file not found, bad seek etc)
     */
    bool fillToMag( float maglim );

    /**
     *@short Sets the first StarBlock in the list to point to the given StarBlock
     *
     *This function must ideally be used only once. Also, it does not make a copy
     *of the StarBlock supplied, but uses the pointer directly. StarBlockList will
     *take care of deleting the StarBlock when it is destroyed
     *
     *@param Pointer to the StarBlock
     */
    void setStaticBlock( StarBlock *block );

    /**
     *@short  Drops the StarBlock with the given pointer from the list
     *@param  Pointer to the StarBlock to remove
     *@return Number of entries removed from the QList
     */
    int releaseBlock( StarBlock *block );

    /**
     *@short  Returns the i-th block in this StarBlockList
     *
     *@param  Index of the required block
     *@return The StarBlock requested for, NULL if index out of bounds
     */
    inline StarBlock *block( unsigned int i ) { return ( ( i < nBlocks ) ? blocks[ i ] : NULL ); }

    /**
     *@short  Returns the total number of stars in this StarBlockList
     *@return Total number of stars in this StarBlockList
     */
    inline long getStarCount() const { return nStars; }

    /**
     *@short  Returns the total number of blocks in theis StarBlockList
     *@return Number of blocks in this StarBlockList
     */
    inline int getBlockCount() const { return nBlocks; }

    /**
     *@short  Returns the magnitude of the faintest star currently stored
     *@return Magnitude of faintest star stored in this StarBlockList
     */
    inline float getFaintMag() const { return faintMag; }

    /**
     *@short  Returns the trixel that this SBL is meant for
     *@return The value of trixel
     */
    inline Trixel getTrixel() const { return trixel; }

 private:
    Trixel trixel;
    unsigned long nStars;
    long readOffset;
    float faintMag;
    QList < StarBlock *> blocks;
    unsigned int nBlocks;
    bool staticStars;
    DeepStarComponent *parent;

};

#endif
