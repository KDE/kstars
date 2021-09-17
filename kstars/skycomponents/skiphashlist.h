/*
    SPDX-FileCopyrightText: 2007 James B. Bowlin <bowlin@mindspring.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "linelist.h"

/**
 * @class SkipList
 * Extends LineList by adding the skip hash to allow the same the data in a
 * LineList to be drawn as a filled and an outlined polygon.
 *
 * NOTE: there is no skiplist.cpp file.  This is all there is.
 *
 * @author James B. Bowlin
 * @version 0.1
 */
class SkipHashList : public LineList
{
  public:
    /**
     * @short returns the entire skip hash.
     * Used by the indexLines() routine so all the line segments in
     * this SkipList can be indexed at once.
     */
    IndexHash *skipHash() { return &m_skip; }

    /**
     * @short instructs that the ith line segment should
     * be skipped when drawn (and hence when indexed too).
     */
    void setSkip(int i) { m_skip[i] = true; }

    /** @short returns the skip flag for the i-th line segment. */
    bool skip(int i) { return m_skip.contains(i); }

  private:
    IndexHash m_skip;
};
