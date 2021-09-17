/*
    SPDX-FileCopyrightText: 2007 James B. Bowlin <bowlin@mindspring.com>
    SPDX-License-Identifier: BSD-3-Clause AND GPL-2.0-or-later
*/

#ifndef MESH_BUFFER_H
#define MESH_BUFFER_H

#include "typedef.h"

class HTMesh;

/** @class MeshBuffer
 * The sole purpose of a MeshBuffer is to hold storage space
 * for the results of an HTM inetersection and then allow multiple
 * MeshIterator's to walk through the result set.  The buffer space is allocated
 * when the MeshBuffer is created.  Mesh buffers will usually hang around for
 * the life of an HTMesh.  Each mesh buffer is re-usable.  Simply reset() it and
 * then fill it by append()'ing trixels.  A MeshIterator grabs the size() and
 * the buffer() so it can iterate over the results.
 */

class MeshBuffer
{
  public:
    MeshBuffer(HTMesh *mesh);

    ~MeshBuffer();

    /** @short prepare the buffer for a new result set
         */
    void reset() { m_size = m_error = 0; }

    /** @short add trixels to the buffer
         */
    int append(Trixel trixel);

    /** @short the location of the buffer for reading
         */
    const Trixel *buffer() const { return m_buffer; }

    /** @short the number of trixels in the result set
         */
    int size() const { return m_size; }

    /** @short returns the number of trixels that would have overflowed the
         * buffer.
         */
    int error() const { return m_error; }

    /** @short fills the buffer with consecutive integers
         */
    void fill();

  private:
    Trixel *m_buffer;
    int m_size;
    int maxSize;
    int m_error;
};

#endif
