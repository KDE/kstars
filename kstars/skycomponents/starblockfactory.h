/***************************************************************************
                 starblockfactory.h  -  K Desktop Planetarium
                             -------------------
    begin                : 7 Jun 2008
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

#ifndef STARBLOCKFACTORY_H
#define STARBLOCKFACTORY_H

#include "typedef.h"
#include "starblock.h"

/**
 *@class StarBlockFactory
 *
 *@short A factory that creates StarBlocks and recycles them in an LRU Cache
 *@author Akarsh Simha
 *@version 0.1
 */

class StarBlockFactory {

 public:

    static StarBlockFactory *Instance();

    /**
     * Destructor
     * Deletes the linked list that maintains the Cache, sets the pointer to NULL
     */
    ~StarBlockFactory();


    /**
     *@short  Return a StarBlock available for use
     *
     *This method first checks if there are any cached StarBlocks that are not in use.
     *If such a StarBlock is found, it returns the same for use. Else it freshly allocates
     *a StarBlock and returns the same. It also moves the StarBlock to the front, marking it
     *as the most recently used. If the StarBlock had a parent StarBlockList, this method
     *detaches the StarBlock from the StarBlockList
     * 
     *@return A StarBlock that is available for use
     */
    StarBlock *getBlock();

    /**
     *@short  Mark a StarBlock as most recently used and sync its drawID with the current drawID
     *
     *@return true on success, false if the StarBlock supplied was not on our list at all
     */
    bool markFirst( StarBlock *block );

    /**
     *@short  Rank a given StarBlock after another given StarBlock in the LRU list
     * and sync its drawID with the current drawID
     *
     *@param  after  The block after which 'block' should be put
     *@param  block  The block to mark for use
     *@return true on success, false on failure
     */
    bool markNext( StarBlock *after, StarBlock *block );

    /**
     *@short  Returns the number of StarBlocks currently produced
     *
     *@return Number of StarBlocks currently allocated
     */
    inline int getBlockCount() { return nBlocks; }

    /**
     *@short  Frees all StarBlocks that are in the cache
     *@return The number of StarBlocks freed
     */
    inline int freeAll() { return deleteBlocks( nBlocks ); }

    /**
     *@short  Frees all StarBlocks that are not used in this draw cycle
     *@return The number of StarBlocks freed
     */
    int freeUnused();

    /**
     *@short  Prints the structure of the cache, for debugging
     */
    void printStructure();

    quint32 drawID;            // A number identifying the current draw cycle

 private:

    /**
     * Constructor
     * Initializes first and last StarBlock pointers to NULL
     */
    StarBlockFactory();

    /**
     * Constructor
     *@short Creates a cache containing N blocks
     *@param nblocks  Number of blocks to allocate
     */
    StarBlockFactory( int nblocks );

    /**
     *@short  Deletes the N least recently used blocks
     *
     *@param  nblocks  Number of blocks to delete
     *@return Number of blocks successfully deleted
     */
    int deleteBlocks( int nblocks );

    StarBlock *first, *last;   // Pointers to the beginning and end of the linked list
    int nBlocks;               // Number of blocks we currently have in the cache
    int nCache;                // Number of blocks to start recycling cached blocks at

    static StarBlockFactory *pInstance;

};

#endif
