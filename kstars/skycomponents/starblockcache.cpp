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

#include "starblockcache.h"


StarBlockCache::StarBlockCache() {
    first = NULL;
    last = NULL;
    nstarblocks = 0;
    useID = -1;
}

StarBlockCache::StarBlockCache(int nblocks) {
    first = NULL;
    last = NULL;
    nstarblocks = 0;
    useID = -1;
    addNewBlocks(nblocks);
}

StarBlockCache::~StarBlockCache() {
    deleteBlocks(nstarblocks);
}

int StarBlockCache::addNewBlocks(int nblocks) {
    int i = 0;

    for(i = 0; i <  nblocks; ++i) {
	if( last == NULL ) {
	    first = last = (StarBlock *) malloc(sizeof(StarBlock));
	    if(first == NULL)
		return 0;
	    first -> prev = NULL;
	}
	else {
	    last -> next = (StarBlock *) malloc(sizeof(StarBlock));
	    if(last -> next == NULL)
		return i;
	    last -> next -> prev = last;
	    last = last -> next;
	}

	last -> next = NULL;
	last -> useID = -1;
	last -> star = NULL;
	last -> parent = NULL;
	last -> nstars = 0;
	last -> brightMag = last -> faintMag = -5.0;
    }

    nstarblocks += i;
    return i;
}

StarBlock *StarBlockCache::getBlock() {

    if(last -> useID == -1 || last -> useID != useID) {
	useBlock(last);
	return first;
    }
    else
	if(addNewBlocks(1)) {
	    useBlock(last);
	    return first;
	}
	else
	    return NULL;

}

bool StarBlockCache::useBlock(StarBlock *block) {

    if(!block)
	return false;

    if(!first)
	return false;

    if(!block -> prev) { // Block is already in the front
	block -> useID = useID;
	return true;
    }

    block -> prev -> next = block -> next;
    if( block -> next )
	block -> next -> prev = block -> prev;
    first -> prev = block;
    block -> next = first;
    first = block;

    block -> useID = useID;

    return true;
}


bool StarBlockCache::groupMove(StarBlock *start, const int nblocks) {

    StarBlock *end;

    // Check for trivial cases
    if(!start || nblocks < 0)
	return false;

    if(nblocks == 0)
	return true;

    if(!first)
	return false;

    // Check for premature end
    end = start;
    for(int i = 1; i < nblocks; ++i, end = end -> next) {
	if(end  == NULL)
	    return false;
    }
    if(end == NULL)
	return false;

    // Update useIDs
    end = start;
    for(int i = 1; i < nblocks; ++i, end = end -> next)
	end -> useID = useID;
    end -> useID = useID;

    // Check if we are already in the front
    if(!start -> prev)
	return true;

    start -> prev -> next = end -> next;
    end -> next -> prev = start -> prev;

    first -> prev = end;
    end -> next = first;
    start -> prev = NULL;
    first = start;
}

int StarBlockCache::deleteBlocks(int nblocks) {
    int i;
    StarBlock *temp;

    i = 0;
    while(last != NULL || i == nblocks) {
	temp = last -> prev;
	free(last);
	last = temp;
	i++;
    }	
    if(last)
	last -> next = NULL;
    else
	first = NULL;

    nstarblocks -= i;
    return i;
}
