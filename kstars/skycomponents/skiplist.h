/***************************************************************************
                          skiplist.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2007-07-06
    copyright            : (C) 2007 by James B. Bowlin
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

#ifndef SKIPLIST_H
#define SKIPLIST_H

#include <QHash>

#include "linelist.h"

/* @class SkipList
 * Extends LineList by adding the skip hash to allow the same the data in a
 * LineList to be drawn as a filled and an outlined polygon.
 *
 * NOTE: there is no skiplist.cpp file.  This is all there is.
 *
 * @author James B. Bowlin
 * @version 0.1
 */
class SkipList : public LineList
{
public:
    /* @short returns the entire skip hash.  Used by the
     * indexLines() routine so all the line segments in
     * this SkipList can be indexed at once.
     */
    IndexHash* skipHash() { return &m_skip; }

    /* @short instructs that the ith line segment should
     * be skipped when drawn (and hence when indexed too).
     */
    void setSkip( int i ) { m_skip[ i ] = true; }

    /* @short returns the skip flag for the i-th line
     * segment.
     */
    bool skip( int i ) { return m_skip.contains( i );  }

private:
    IndexHash m_skip;

};

#endif
