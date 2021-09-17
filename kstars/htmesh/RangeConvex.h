#ifndef _RangeConvex_h
#define _RangeConvex_h

//#     Filename:       RangeConvex.h
//#
//#     Classes defined here: RangeConvex
//#
//#
//#     Author:         Peter Z. Kunszt, based on A. Szalay's code
//#
//#     Date:           October 16, 1998
//#
//#	    SPDX-FileCopyrightText: 2000 Peter Z. Kunszt Alex S. Szalay, Aniruddha R. Thakar
//#                     The Johns Hopkins University
//#
//#     Modification History:
//#
//#     Oct 18, 2001 : Dennis C. Dinge -- Replaced ValVec with std::vector
//#

class RangeConvex;
class SpatialConstraint;
class SpatialSign;

#include "SpatialConstraint.h"
#include "SpatialIndex.h"
#include "SpatialSign.h"

#include <HtmRange.h>

typedef std::vector<uint64> ValueVectorUint64;

/** Enumerator. Define the return values of an intersection */

enum SpatialMarkup
{
    /// Uncertain
    dONTKNOW,
    /// Triangle partially intersected
    pARTIAL,
    /// All of the triangle is inside queried area
    fULL,
    /// Triangle is outside the queried area
    rEJECT
};

//########################################################################
//#
//# Spatial Convex class
//#
/**
   A spatial convex is composed of spatial constraints. It does not
   necessarily define a continuous area on the sphere since it is a
   3D-convex of planar intersections which may intersect with the
   unit sphere at disjoint locations. Especially 'negative'
   constraints tend to tear 'holes' into the convex area.
*/

class LINKAGE RangeConvex
{
  public:
    /// Default Constructor
    RangeConvex();

    /// Constructor from a triangle
    RangeConvex(const SpatialVector *v1, const SpatialVector *v2, const SpatialVector *v3);

    /// Constructor from a rectangle
    RangeConvex(const SpatialVector *v1, const SpatialVector *v2, const SpatialVector *v3, const SpatialVector *v4);

    /// Add a constraint
    void add(SpatialConstraint &);

    /// Simplify the convex, remove redundancies
    void simplify();

    /** Intersect with index. Result is given in a list of nodes. */
    void intersect(const SpatialIndex *index, HtmRange *hr);

    void setOlevel(int level) { olevel = level; };

  protected:
    HtmRange *hr;
    int olevel;
    Sign sign_ { zERO };

    // Do the intersection (common function for overloaded intersect())
    // Simplification routine for zERO convexes. This is called by
    // simplify() in case we have all zERO constraints.
    void simplify0();

    // saveTrixel: saves the given htmid for later output
    void saveTrixel(uint64 htmid);

    // testTrixel: Test the nodes of the index if the convex hits it
    // the argument gives the index of the nodes_ array to specify the QuadNode
    SpatialMarkup testTrixel(uint64 nodeIndex);

    // test each quadnode for intersections. Calls testTriangle after having
    // tested the vertices using testVertex.
    SpatialMarkup testNode(uint64 id);

    // SpatialMarkup testNode(const struct SpatialIndex::QuadNode *indexNode);
    SpatialMarkup testNode(const SpatialVector &v0, const SpatialVector &v1, const SpatialVector &v2);

    // testTriangle: tests a triangle given by 3 vertices if
    // it intersects the convex. Here the whole logic of deciding
    // whether it is partial, full, swallowed or unknown is handled.
    SpatialMarkup testTriangle(const SpatialVector &v0, const SpatialVector &v1, const SpatialVector &v2, int vsum);

    // test a triangle's subtriangles whether they are partial.
    // If level is nonzero, recurse: subdivide the
    // triangle according to our rules and test each subdivision.
    // (our rules are: each subdivided triangle has to be given
    // ordered counter-clockwise, 0th index starts off new 0-node,
    // 1st index starts off new 1-node, 2nd index starts off new 2-node
    // middle triangle gives new 3-node.)
    // if we are at the bottom, set this id's leafindex in partial bitlist.
    void testPartial(size_t level, uint64 id, const SpatialVector &v0, const SpatialVector &v1, const SpatialVector &v2,
                     int previousPartials);

    // Test for constraint relative position; intersect, one in the
    // other, disjoint.
    int testConstraints(size_t i, size_t j);

    // Test if vertices are inside the convex for a node.
    int testVertex(const SpatialVector &v);
    int testVertex(const SpatialVector *v);

    // testHole : look for 'holes', i.e. negative constraints that have their
    // centers inside the node with the three corners v0,v1,v2.
    bool testHole(const SpatialVector &v0, const SpatialVector &v1, const SpatialVector &v2);

    // testEdge0: test the edges of the triangle against the edges of the
    // zERO convex. The convex is stored in corners_ so that the convex
    // is always on the left-hand-side of an edge corners_(i) - corners_(i+1).
    // (just like the triangles). This makes testing for intersections with
    // the edges easy.
    bool testEdge0(const SpatialVector &v0, const SpatialVector &v1, const SpatialVector &v2);

    // testEdge: look whether one of the constraints intersects with one of
    // the edges of node with the corners v0,v1,v2.
    bool testEdge(const SpatialVector &v0, const SpatialVector &v1, const SpatialVector &v2);

    // eSolve: solve the quadratic equation for the edge v1,v2 of
    // constraint[cIndex]
    bool eSolve(const SpatialVector &v1, const SpatialVector &v2, size_t cIndex);

    // Test if bounding circle intersects with a constraint
    bool testBoundingCircle(const SpatialVector &v0, const SpatialVector &v1, const SpatialVector &v2);

    // Test if a constraint intersects the edges
    bool testEdgeConstraint(const SpatialVector &v0, const SpatialVector &v1, const SpatialVector &v2, size_t cIndex);

    // Look for any positive constraint that does not intersect the edges
    size_t testOtherPosNone(const SpatialVector &v0, const SpatialVector &v1, const SpatialVector &v2);

    // Test for a constraint lying inside or outside of triangle
    bool testConstraintInside(const SpatialVector &v0, const SpatialVector &v1, const SpatialVector &v2, size_t cIndex);

    // Test for a vector lying inside or outside of triangle
    bool testVectorInside(const SpatialVector &v0, const SpatialVector &v1, const SpatialVector &v2, SpatialVector &v);

    std::vector<SpatialConstraint> constraints_; // The vector of constraints
    const SpatialIndex *index_;                  // A pointer to the index
    std::vector<SpatialVector> corners_;
    SpatialConstraint boundingCircle_; // For zERO convexes, the bc.
    size_t addlevel_;                  // additional levels to calculate
    ValueVectorUint64 *plist_;         // list of partial node ids
  private:
    // Disallow copying and assignemnt
    RangeConvex(const RangeConvex &);
    RangeConvex &operator=(const RangeConvex &);
};

#endif
