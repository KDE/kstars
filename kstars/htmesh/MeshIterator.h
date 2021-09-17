/*
    SPDX-FileCopyrightText: 2007 James B. Bowlin <bowlin@mindspring.com>
    SPDX-License-Identifier: BSD-3-Clause AND GPL-2.0-or-later
*/

#pragma once

#include "typedef.h"

class HTMesh;

/**
 * @class MeshIterator
 * MeshIterator is a very lightweight class used to iterate over the
 * result set of an HTMesh intersection.  If you want to iterate over the same
 * result set multiple times in the same block of code, you don't need to create
 * a new MeshIterator, just call the reset() method and then re-use the iterator.
 */

class MeshIterator
{
  public:
    MeshIterator(HTMesh *mesh, BufNum bufNum = 0);

    /** @short true if there are more trixel to iterate over.
         */
    bool hasNext() const { return cnt < m_size; }

    /** @short returns the next trixel
         */
    Trixel next() const { return index[cnt++]; }

    /** @short returns the number of trixels stored
         */
    int size() const { return m_size; }

    /** @short sets the count back to zero so you can use this iterator
         * to iterate again over the same result set.
         */
    void reset() const { cnt = 0; }

  private:
    const Trixel *index;
    int m_size;
    mutable int cnt;
};
