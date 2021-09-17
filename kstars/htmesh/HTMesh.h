/*
    SPDX-FileCopyrightText: 2007 James B. Bowlin <bowlin@mindspring.com>
    SPDX-License-Identifier: BSD-3-Clause AND GPL-2.0-or-later
*/

// FIXME? We are needlessly copying the trixel data into the buffer during
// indexing.  One way to fix this is to give HTMesh next() and hasNext()
// methods.  This would also involve moving the convex and the range off the
// stack and back to the heap when the indexing methods are called for indexing
// and not drawing.  Let's wait.

#ifndef HTMESH_H
#define HTMESH_H

#include <cstdio>
#include "typedef.h"

class SpatialIndex;
class RangeConvex;
class MeshIterator;
class MeshBuffer;

/**
 * @class HTMesh
 * HTMesh was originally intended to be a simple interface to the HTM
 * library for the KStars project that would hide some of the complexity.  But
 * it has gained some complexity of its own.
 *
 * The most complex addition was the routine that performs an intersection on a
 * line segment, finding all the trixels that cover the line.  Perhaps there is
 * an easier and faster way to do it.  Right now we convert to cartesian
 * coordinates to take the cross product of the two input vectors in order to
 * create a perpendicular which is used to form a very narrow triangle that
 * contains the original line segment as one of its long legs.
 *
 * Error detection and prevention added a little bit more complexity.  The raw
 * HTM library is vulnerable to misbehavior if the polygon intersection routines
 * are given duplicate consecutive points, including the first point of a
 * polygon duplicating the last point.  We test for duplicated points now.  If
 * they are found, we drop down to the intersection routine with one fewer
 * vertices, ending at the line segment.  If the two ends of a line segment are
 * much close together than the typical edge of a trixel then we simply index
 * each point separately and the result set is consists of the union of these
 * two trixels.
 *
 * The final (I hope) level of complexity was the addition of multiple results
 * buffers.  You can set the number of results buffers in the HTMesh
 * constructor.  Since each buffer has to be able to hold one integer for each
 * trixel in the mesh, multiple buffers can quickly eat up memory.  The default
 * is just one buffer and all routines that use the buffers default to using the
 * just the first buffer.
 *
 * NOTE: all Right Ascensions (ra) and Declinations (dec) are in degrees.
 */

class HTMesh
{
  public:
    /** @short constructor.
         * @param level is passed on to the underlying SpatialIndex
         * @param buildLevel is also passed on to the SpatialIndex
         * @param numBuffers controls how many output buffers are created. Don't
         * use more than require because they eat up mucho RAM.  The default is
         * just one output buffer.
         */
    HTMesh(int level, int buildLevel, int numBuffers = 1);

    ~HTMesh();

    /** @short returns the index of the trixel that contains the specified
         * point.
         */
    Trixel index(double ra, double dec) const;

    /** NOTE: The intersect() routines below are all used to find the trixels
         * needed to cover a geometric object: circle, line, triangle, and
         * quadrilateral.  Since the number of trixels needed can be large and is
         * not known a priori, you must construct a MeshIterator to iterate over
         * the trixels that are the result of any of these 4 calculations.
         *
         * The HTMesh is created with one or more output buffers used for storing
         * the results. You can specify which output buffer is to be used to hold
         * the results by supplying an optional integer bufNum parameter.
         */

    /**
         *@short finds the trixels that cover the specified circle
         *@param ra Central ra in degrees
         *@param dec Central dec in degrees
         *@param radius Radius of the circle in degrees
         *@param bufNum the output buffer to hold the results
         *@note You will need to supply unprecessed (ra, dec) in most
         * situations. Please see SkyMesh::aperture()'s code before
         * messing with this method.
         */
    void intersect(double ra, double dec, double radius, BufNum bufNum = 0);

    /** @short finds the trixels that cover the specified line segment
         */
    void intersect(double ra1, double dec1, double ra2, double dec2, BufNum bufNum = 0);

    /** @short find the trixels that cover the specified triangle
         */
    void intersect(double ra1, double dec1, double ra2, double dec2, double ra3, double dec3, BufNum bufNum = 0);

    /** @short finds the trixels that cover the specified quadrilateral
         */
    void intersect(double ra1, double dec1, double ra2, double dec2, double ra3, double dec3, double ra4, double dec4,
                   BufNum bufNum = 0);

    /** @short returns the number of trixels in the result buffer bufNum.
         */
    int intersectSize(BufNum bufNum = 0);

    /** @short returns the total number of trixels in the HTM.  This number
         * is 8 * 4^level.
         */
    int size() const { return numTrixels; }

    /** @short returns the mesh level.
         */
    int level() const { return m_level; }

    /** @short sets the debug level which is used to print out intermediate
         * results in the line intersection routine.
         */
    void setDebug(int debug) { htmDebug = debug; }

    /** @short returns  a pointer to the MeshBuffer specified by bufNum.
         * Currently this is only used in the MeshIterator constructor.
         */
    MeshBuffer *meshBuffer(BufNum bufNum = 0);

    void vertices(Trixel id, double *ra1, double *dec1, double *ra2, double *dec2, double *ra3, double *dec3);

  private:
    const char *name;
    SpatialIndex *htm;
    int m_level, m_buildLevel;
    int numTrixels, magicNum;

    // These store the result sets:
    MeshBuffer **m_meshBuffer;
    BufNum m_numBuffers;

    double degree2Rad;
    double edge, edge10, eps;

    int htmDebug;

    /** @short fills the specified buffer with the intersection results in the
         * RangeConvex.
         */
    bool performIntersection(RangeConvex *convex, BufNum bufNum = 0);

    /** @short users can only use the allocated buffers
         */
    inline bool validBufNum(BufNum bufNum)
    {
        if (bufNum < m_numBuffers)
            return true;
        fprintf(stderr, "%s: bufNum: %d >= numBuffers: %d\n", name, bufNum, m_numBuffers);
        return false;
    }

    /** @short used by the line intersection routine.  Maybe there is a
         * simpler and faster approach that does not require this conversion.
         */
    void toXYZ(double ra, double dec, double *x, double *y, double *z);
};

#endif
