/***************************************************************************
                           skiplistindex.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2007-07-08
    copyright            : (C) 2007 James B. Bowlin
    email                : bowlin@mindspring.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef SKIPLISTINDEX_H
#define SKIPLISTINDEX_H

#include "linelistindex.h"
#include "skiplist.h"

/* @class SkipListIndex
 * This class should store SkipLists instead of LineLists.  The two methods
 * are used inside of LineListIndex to access the SkipLists' skip hashes.
 * This way the same code in LineListIndex does double duty. Only subclassed
 * by MilkyWay.
 *
 * @author James B. Bowlin
 * @version 0.1
 */

class SkipListIndex : public LineListIndex
{
public:
    /*
     * @short Constructor
     */
    SkipListIndex( SkyComponent *parent, const QString& name );

    /* @short Returns an IndexHash from the SkyMesh that contains the set
     * of trixels that cover the _SkipList_ lineList excluding skipped
     * lines as specified in the SkipList.  SkipList is a subclass of
     * LineList.
     */
    const IndexHash& getIndexHash( LineList* skipList );

    /* @short Returns a boolean indicating whether to skip the i-th line
     * segment in the _SkipList_ skipList.  Note that SkipList is a
     * subclass of LineList.  This routine allows us to use the drawing
     * code in LineListIndex instead of repeating it all here.
     */
    bool skipAt( LineList* skpiList, int i );

};

#endif
