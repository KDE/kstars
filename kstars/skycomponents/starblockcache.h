/***************************************************************************
                 starblockcache.h  -  K Desktop Planetarium
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

#ifndef STARBLOCKCACHE_H
#define STARBLOCKCACHE_H

#include "typedef.h"
#include "starblock.h"

/**
 *@class StarBlockCache
 *
 *Maintains a Least Recently Used Cache of StarBlocks
 *
 *@author Akarsh Simha
 *@version 0.1
 */

class StarBlockCache {

 public:

    /**
     * Constructor
     * Initializes first and last StarBlock pointers to NULL
     */
    StarBlockCache();

    /**
     * Constructor
     *@short Creates a cache containing N blocks
     *@param nblocks  Number of blocks to allocate
     */
    StarBlockCache( int nblocks );


    /**
     * Destructor
     * Deletes the linked list that maintains the Cache
     */
    ~StarBlockCache();

    /**
     *@short  Appends N new blocks to the end of the list
     *
     *@param  Number of blocks to append
     *@return Number of blocks successfully appended
     */
    int addNewBlocks( int nblocks );

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
    bool useBlock( StarBlock *block );

    /**
     *@short  Move a group of blocks as they are to the front
     *
     *This function moves a group of blocks, maintaining the links within the group as they
     *are, to the front. This is useful if the whole block was used in an operation, but the
     *last used block should be the first dropped block, if the cache need to drop a block
     *
     *@param  start   Pointer to the start of the group to move
     *@param  nblocks Number of blocks to include in the group
     *@return true on success, false if the group was not correctly defined
     */
    bool groupMove( StarBlock *start, const int nblocks );

    /**
     *@short  Deletes the N least recently used blocks
     *
     *@param  nblocks  Number of blocks to delete
     *@return Number of blocks successfully deleted
     */
    int deleteBlocks( int nblocks );

    quint32 drawID;            // A number identifying the current 'use' cycle

 private:

    /**
     *@short  Update drawIDs of nblocks blocks starting at start
     *
     *@param  start   Pointer to the start of the group to sync drawIDs of
     *@param  nblocks Number of blocks in the group
     */

    StarBlock *first, *last;   // Pointers to the beginning and end of the linked list
    int nstarblocks;           // Number of blocks we currently have in the cache

};

#endif
