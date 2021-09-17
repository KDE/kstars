//# Filename: RangeConvex.cpp # # The RangeConvex # classes are
//defined here.  # # Author: Peter Z. Kunszt based on A. Szalay's code
//# # Date: October 23, 1998 # # SPDX-FileCopyrightText: 2000 Peter Z. Kunszt
//Alex S. Szalay, Aniruddha R. Thakar # The Johns Hopkins University #
//# Modification History: # # Oct 18, 2001 : Dennis C. Dinge --
//Replaced ValVec with std::vector

#include "RangeConvex.h"

#include "SpatialGeneral.h"

#define N(n)     index_->nodes_[(n)]
#define NC(n, m) index_->nodes_[(n)].childID_[(m)] // the children n->m
#define NV(m)    index_->nodes_[id].v_[(m)]        // the vertices of n
#define V(m)     index_->vertices_[(m)]            // the vertex vector m

#define SGN(x) ((x) < 0 ? -1 : ((x) > 0 ? 1 : 0)) // signum

// ===========================================================================
//
// Member functions for class RangeConvex
//
// ===========================================================================

RangeConvex::RangeConvex()
{
}

/////////////CONSTRUCTOR FROM A TRIANGLE//////////////////
//
// Initialize convex from a triangle. The corners of these vectors
// form a triangle, so we just add three ZERO constraints to the convex
// where the direction is given by the cross product of the corners.
// Of course, the sign has to be determined (on which side of the triangle
// are we?) If the three points lie on one line, no constraints are added.
//
RangeConvex::RangeConvex(const SpatialVector *v1, const SpatialVector *v2, const SpatialVector *v3)
{
    SpatialVector a1 = (*v2) ^ (*v3); // set directions of half-spheres
    SpatialVector a2 = (*v3) ^ (*v1);
    SpatialVector a3 = (*v1) ^ (*v2);
    float64 s1       = a1 * (*v1); // we really need only the signs of these
    float64 s2       = a2 * (*v2);
    float64 s3       = a3 * (*v3);

    if (s1 * s2 * s3 != 0) // this is nonzero if not on one line
    {
        if (s1 < 0.0L)
            a1 = (-1) * a1; // change sign if necessary
        if (s2 < 0.0L)
            a2 = (-1) * a2;
        if (s3 < 0.0L)
            a3 = (-1) * a3;
        constraints_.push_back(SpatialConstraint(a1, 0.0)); // we don't care about the
        constraints_.push_back(SpatialConstraint(a2, 0.0)); // order since all angles are
        constraints_.push_back(SpatialConstraint(a3, 0.0)); // 90 degrees.
    }
    sign_ = zERO;
}

/////////////CONSTRUCTOR FROM A RECTANGLE/////////////////
//
// Initialize convex from a rectangle. The vectors that form a rectangle
// may be in any order, the code finds the edges by itself.
// If one of the vectors lies within the triangle formed by the
// other three vectors, the previous constructor is used.
//
RangeConvex::RangeConvex(const SpatialVector *v1, const SpatialVector *v2, const SpatialVector *v3,
                         const SpatialVector *v4)
{
    int i, j, k, l, m; // indices
    // to simplify things, copy input into a 4-array
    const SpatialVector *v[4] = { v1, v2, v3, v4 };
    SpatialVector d[6];
    float64 s[6][2];
    for (i = 0, k = 0; i < 4; i++)
        for (j = i + 1; j < 4; j++, k++) // set directions of half-spheres
        {
            d[k] = (*v[i]) ^ (*v[j]); // two of these are diagonals.
            d[k].normalize();
            for (l = 0, m = 0; l < 4; l++)
                if (l != i && l != j)
                    s[k][m++] = d[k] * (*v[l]); // set the 'sign'
        }

    // the sides are characterized by having both other corners
    // to the same (inner) side. so it is easy to find the edges.
    // again, the sign has to be taken care of -> direction of d
    // the nice thing here is that if one of the corners is inside
    // a triangles formed by the other three, only 3 constraints get
    // added.
    for (i = 0; i < 6; i++)
        if (s[i][0] * s[i][1] > 0.0) // not >= because we don't want aligned corners
            constraints_.push_back(SpatialConstraint((s[i][0] > 0.0 ? d[i] : (-1 * d[i])), 0.0));

    // Special cases: 1
    // if three of the corners are aligned, we end up with
    // only two constraints. Find the third and append it.
    // Indeed, there are 3 identical constraints among the d[],
    // so the first that qualifies gets appended.
    if (constraints_.size() == 2)
    {
        for (i = 0; i < 6; i++)
            if (s[i][0] == 0.0 || s[i][1] == 0.0)
            {
                constraints_.push_back(SpatialConstraint(((s[i][0] + s[i][1]) > 0.0 ? d[i] : (-1 * d[i])), 0.0));
                break;
            }
    }
    // Special cases: 2
    // if all four corners are aligned, no constraints have been appended.
    sign_ = zERO;
}

/////////////ADD//////////////////////////////////////////
//
void RangeConvex::add(SpatialConstraint &c)
{
    constraints_.push_back(c);
    // order constraints: by ascending opening angle. Since we append
    // always at the end, we only need one ordering sweep starting at
    // the end
    for (size_t i = constraints_.size() - 1; i > 0; i--)
    {
        if (constraints_[i].s_ < constraints_[i - 1].s_)
        {
            SpatialConstraint tmp(constraints_[i]);
            constraints_[i]     = constraints_[i - 1];
            constraints_[i - 1] = tmp;
        }
    }

    if (constraints_.size() == 1) // first constraint
    {
        sign_ = c.sign_;
        return;
    }

    switch (sign_)
    {
        case nEG:
            if (c.sign_ == pOS)
                sign_ = mIXED;
            break;
        case pOS:
            if (c.sign_ == nEG)
                sign_ = mIXED;
            break;
        case zERO:
            sign_ = c.sign_;
            break;
        case mIXED:
            break;
    }
}

/////////////SIMPLIFY0////////////////////////////////////
// simplify0: simplify zERO convexes. calculate corners of convex
// and the bounding circle.
//
// zERO convexes are made up of constraints which are all great
// circles. It can happen that some of the constraints are redundant.
// For example, if 3 of the great circles define a triangle as the convex
// which lies fully inside the half sphere of the fourth constraint,
// that fourth constraint is redundant and will be removed.
//
// The algorithm is the following:
//
// zero-constraints are half-spheres, defined by a single normalized
// vector v, pointing in the direction of that half-sphere.
//
// Two zero-constraints intersect at
//
//    i    =  +- v  x v
//     1,2        1    2
//
// the vector cross product of their two defining vectors.
//
// The two vectors i1,2 are tested against every other constraint in
// the convex if they lie within their half-spheres. Those
// intersections i which lie within every other constraint, are stored
// into corners_.
//
// Constraints that do not have a single corner on them, are dropped.
//

void RangeConvex::simplify0()
{
    size_t i, j, k;
    SpatialVector vi1, vi2;
    std::vector<size_t> cornerConstr1, cornerConstr2, removeConstr;
    std::vector<SpatialVector> corner;
    if (constraints_.size() == 1) // for one constraint, it is itself the BC
    {
        boundingCircle_ = constraints_[0];
        return;
        // For 2 constraints, take the bounding circle a 0-constraint...
        // this is by no means optimal, but the code is optimized for at least
        // 3 zERO constraints... so this is acceptable.
    }
    else if (constraints_.size() == 2)
    {
        // test for constraints being identical - rule 1 out
        if (constraints_[0].a_ == constraints_[1].a_)
        {
            constraints_.erase(constraints_.end() - 1);
            boundingCircle_ = constraints_[0];
            return;
        }
        // test for constraints being two disjoint half spheres - empty convex!
        if (constraints_[0].a_ == (-1.0) * constraints_[1].a_)
        {
            constraints_.clear();
            return;
        }
        boundingCircle_ = SpatialConstraint(constraints_[0].v() + constraints_[1].v(), 0);
        return;
    }

    // Go over all pairs of constraints
    for (i = 0; i < constraints_.size() - 1; i++)
    {
        bool ruledout = true;
        for (j = i + 1; j < constraints_.size(); j++)
        {
            // test for constraints being identical - rule i out
            if (constraints_[i].a_ == constraints_[j].a_)
                break;
            // test for constraints being two disjoint half spheres - empty convex!
            if (constraints_[i].a_ == (-1.0) * constraints_[j].a_)
            {
                constraints_.clear();
                return;
            }
            // vi1 and vi2 are their intersection points
            vi1 = constraints_[i].a_ ^ constraints_[j].a_;
            vi1.normalize();
            vi2        = (-1.0) * vi1;
            bool vi1ok = true, vi2ok = true;
            // now test whether vi1 or vi2 or both are inside every other constraint.
            // if yes, store them in the corner array.
            for (k = 0; k < constraints_.size(); k++)
            {
                if (k == i || k == j)
                    continue;
                if (vi1ok && vi1 * constraints_[k].a_ <= 0.0)
                    vi1ok = false;
                if (vi2ok && vi2 * constraints_[k].a_ <= 0.0)
                    vi2ok = false;
                if (!vi1ok && !vi2ok)
                    break;
            }
            if (vi1ok)
            {
                corner.push_back(vi1);
                cornerConstr1.push_back(i);
                cornerConstr2.push_back(j);
                ruledout = false;
            }
            if (vi2ok)
            {
                corner.push_back(vi2);
                cornerConstr1.push_back(i);
                cornerConstr2.push_back(j);
                ruledout = false;
            }
        }
        // is this constraint ruled out? i.e. none of its intersections
        // with other constraints are corners... remove it from constraints_ list.
        if (ruledout)
            removeConstr.push_back(i);
    }

    // Now set the corners into their correct order, which is an
    // anti-clockwise walk around the polygon.
    //
    // start at any corner. so take the first.

    corners_.clear();
    corners_.push_back(corner[0]);

    // The trick is now to start off into the correct direction.
    // this corner has two edges it can walk. we have to take the
    // one where the convex lies on its left side.
    i = cornerConstr1[0]; // the i'th constraint and j'th constraint
    j = cornerConstr2[0]; // intersect at 0'th corner
    size_t c1(0), c2(0), k1(0), k2(0);
    // Now find the other corner where the i'th and j'th constraints intersect.
    // Store the corner in vi1 and vi2, and the other constraint indices
    // in c1,c2.
    for (k = 1; k < cornerConstr1.size(); k++)
    {
        if (cornerConstr1[k] == i)
        {
            vi1 = corner[k];
            c1  = cornerConstr2[k];
            k1  = k;
        }
        if (cornerConstr2[k] == i)
        {
            vi1 = corner[k];
            c1  = cornerConstr1[k];
            k1  = k;
        }
        if (cornerConstr1[k] == j)
        {
            vi2 = corner[k];
            c2  = cornerConstr2[k];
            k2  = k;
        }
        if (cornerConstr2[k] == j)
        {
            vi2 = corner[k];
            c2  = cornerConstr1[k];
            k2  = k;
        }
    }
    // Now test i'th constraint-edge ( corner 0 and corner k ) whether
    // it is on the correct side (left)
    //
    //  ( (corner(k) - corner(0)) x constraint(i) ) * corner(0)
    //
    // is >0 if yes, <0 if no...
    //
    size_t c, currentCorner;
    if (((vi1 - corner[0]) ^ constraints_[i].a_) * corner[0] > 0)
    {
        corners_.push_back(vi1);
        c             = c1;
        currentCorner = k1;
    }
    else
    {
        corners_.push_back(vi2);
        c             = c2;
        currentCorner = k2;
    }
    // now append the corners that match the index c until we got corner 0 again
    // currentCorner holds the current corners index
    // c holds the index of the constraint that has just been intersected with
    // So:
    // x We are on a constraint now (i or j from before), the second corner
    //   is the one intersecting with constraint c.
    // x Find other corner for constraint c.
    // x Save that corner, and set c to the constraint that intersects with c
    //   at that corner. Set currentcorner to that corners index.
    // x Loop until 0th corner reached.
    while (currentCorner)
    {
        for (k = 0; k < cornerConstr1.size(); k++)
        {
            if (k == currentCorner)
                continue;
            if (cornerConstr1[k] == c)
            {
                if ((currentCorner = k) == 0)
                    break;
                corners_.push_back(corner[k]);
                c = cornerConstr2[k];
                break;
            }
            if (cornerConstr2[k] == c)
            {
                if ((currentCorner = k) == 0)
                    break;
                corners_.push_back(corner[k]);
                c = cornerConstr1[k];
                break;
            }
        }
    }
    // Remove all redundant constraints
    for (i = 0; i < removeConstr.size(); i++)
        constraints_.erase(constraints_.end() - removeConstr[i] - 1);

    // Now calculate the bounding circle for the convex.
    // We take it as the bounding circle of the triangle with
    // the widest opening angle. All triangles made out of 3 corners
    // are considered.
    boundingCircle_.d_ = 1.0;
    if (constraints_.size() >= 3)
    {
        for (i = 0; i < corners_.size(); i++)
            for (j = i + 1; j < corners_.size(); j++)
                for (k = j + 1; k < corners_.size(); k++)
                {
                    SpatialVector v = (corners_[j] - corners_[i]) ^ (corners_[k] - corners_[j]);
                    v.normalize();
                    // Set the correct opening angle: Since the plane cutting
                    // out the triangle also correctly cuts out the bounding cap
                    // of the triangle on the sphere, we can take any corner to
                    // calculate the opening angle
                    float64 d = v * corners_[i];
                    if (boundingCircle_.d_ > d)
                        boundingCircle_ = SpatialConstraint(v, d);
                }
    }
}

/////////////SIMPLIFY/////////////////////////////////////
// simplify: We have the following decision tree for the
//           simplification of convexes:
//
//  Always test two constraints against each other. We have
//
//  * If both constraints are pOS
//     # If they intersect: keep both
//     # If one lies in the other: drop the larger one
//     # Else: disjunct. Empty convex, stop.
//
//  * If both constraints are nEG
//     # If they intersect or are disjunct: ok
//     # Else: one lies in the other, drop smaller 'hole'
//
//  * Mixed: one pOS, one nEG
//     # No intersection, disjunct: pOS is redundant
//     # Intersection: keep both
//     # pOS within nEG: empty convex, stop.
//     # nEG within pOS: keep both.
//

void RangeConvex::simplify()
{
    if (sign_ == zERO)
    {
        simplify0(); // treat zERO convexes separately
        return;
    }

    size_t i, j;
    size_t clen;
    bool redundancy = true;

    while (redundancy)
    {
        redundancy = false;
        clen       = constraints_.size();

        for (i = 0; i < clen; i++)
        {
            for (j = 0; j < i; j++)
            {
                int test;

                // don't bother with two zero constraints
                if (constraints_[i].sign_ == zERO && constraints_[j].sign_ == zERO)
                    continue;

                // both pos or zero
                if ((constraints_[i].sign_ == pOS || constraints_[i].sign_ == zERO) &&
                    (constraints_[j].sign_ == pOS || constraints_[j].sign_ == zERO))
                {
                    if ((test = testConstraints(i, j)) == 0)
                        continue; // intersection
                    if (test < 0) // disjoint ! convex is empty
                    {
                        constraints_.clear();
                        return;
                    }
                    // one is redundant
                    if (test == 1)
                        constraints_.erase(constraints_.end() - i - 1);
                    else if (test == 2)
                        constraints_.erase(constraints_.end() - j - 1);
                    else
                        continue;      // intersection
                    redundancy = true; // we did cut out a constraint -> do the loop again
                    break;
                }

                // both neg or zero
                if ((constraints_[i].sign_ == nEG) && (constraints_[j].sign_ == nEG))
                {
                    if ((test = testConstraints(i, j)) <= 0)
                        continue; // ok
                    // one is redundant
                    if (test == 1)
                        constraints_.erase(constraints_.end() - 1 - j);
                    else if (test == 2)
                        constraints_.erase(constraints_.end() - 1 - i);
                    else
                        continue;      // intersection
                    redundancy = true; // we did cut out a constraint -> do the loop again
                    break;
                }

                // one neg, one pos/zero
                if ((test = testConstraints(i, j)) == 0)
                    continue; // ok: intersect
                if (test < 0) // neg is redundant
                {
                    if (constraints_[i].sign_ == nEG)
                        constraints_.erase(constraints_.end() - 1 - i);
                    else
                        constraints_.erase(constraints_.end() - 1 - j);
                    redundancy = true; // we did cut out a constraint -> do the loop again
                    break;
                }
                // if the negative constraint is inside the positive: continue
                if ((constraints_[i].sign_ == nEG && test == 2) || (constraints_[j].sign_ == nEG && test == 1))
                    continue;
                // positive constraint in negative: convex is empty!
                constraints_.clear();
                return;
            }
            if (redundancy)
                break;
        }
    }

    // reset the sign of the convex
    sign_ = constraints_[0].sign_;
    for (i = 1; i < constraints_.size(); i++)
    {
        switch (sign_)
        {
            case nEG:
                if (constraints_[i].sign_ == pOS)
                    sign_ = mIXED;
                break;
            case pOS:
                if (constraints_[i].sign_ == nEG)
                    sign_ = mIXED;
                break;
            case zERO:
                sign_ = constraints_[i].sign_;
                break;
            case mIXED:
                break;
        }
    }

    if (constraints_.size() == 1) // for one constraint, it is itself the BC
        boundingCircle_ = constraints_[0];
    else if (sign_ == pOS)
        boundingCircle_ = constraints_[0];
}

/////////////TESTCONSTRAINTS//////////////////////////////
// testConstraints: Test for the relative position of two constraints.
//                  Returns 0  if they intersect
//                  Returns -1 if they are disjoint
//                  Returns 1  if j is in i
//                  Returns 2  if i is in j
//
int RangeConvex::testConstraints(size_t i, size_t j)
{
    float64 phi = ((constraints_[i].sign_ == nEG ? (-1 * constraints_[i].a_) : constraints_[i].a_) *
                   (constraints_[j].sign_ == nEG ? (-1 * constraints_[j].a_) : constraints_[j].a_));
    phi         = (phi <= -1.0L + gEpsilon ? gPi : acos(phi)); // correct for math lib -1.0
    float64 a1  = (constraints_[i].sign_ == pOS ? constraints_[i].s_ : gPi - constraints_[i].s_);
    float64 a2  = (constraints_[j].sign_ == pOS ? constraints_[j].s_ : gPi - constraints_[j].s_);

    if (phi > a1 + a2)
        return -1;
    if (a1 > phi + a2)
        return 1;
    if (a2 > phi + a1)
        return 2;
    return 0;
}

/////////////INTERSECT////////////////////////////////////
// used by intersect.cpp application
//
void RangeConvex::intersect(const SpatialIndex *idx, HtmRange *htmrange)
{
    hr        = htmrange;
    index_    = idx;
    addlevel_ = idx->maxlevel_ - idx->buildlevel_;

    simplify(); // don't work too hard...

    if (constraints_.empty())
        return; // nothing to intersect!!

    // Start with root nodes (index = 1-8) and intersect triangles
    for (uint32 i = 1; i <= 8; i++)
    {
        testTrixel(i);
    }
}

////////////SAVETRIXEL
//
inline void RangeConvex::saveTrixel(uint64 htmid)
{
    // Some application want a complete htmid20 range
    //
    int level, i, shifts;
    uint64 lo, hi;
#ifdef _WIN32
    uint64 IDHIGHBIT  = 1;
    uint64 IDHIGHBIT2 = 1;
    IDHIGHBIT         = IDHIGHBIT << 63;
    IDHIGHBIT2        = IDHIGHBIT2 << 62;
#endif

    for (i = 0; i < IDSIZE; i += 2)
    {
        if ((htmid << i) & IDHIGHBIT)
            break;
    }

    level = (IDSIZE - i) >> 1;
    level -= 2;
    if (level < olevel)
    {
        /* Size is the length of the string representing the name of the
           trixel, the level is size - 2
        */
        shifts = (olevel - level) << 1;
        lo     = htmid << shifts;
        hi     = lo + ((uint64)1 << shifts) - 1;
    }
    else
    {
        lo = hi = htmid;
    }
    hr->mergeRange(lo, hi);

    return;
}

/////////////TRIANGLETEST/////////////////////////////////
// testTrixel: this is the main test of a triangle vs a Convex.  It
// will properly mark up the flags for the triangular node[index], and
// all its children
SpatialMarkup RangeConvex::testTrixel(uint64 id)
{
    SpatialMarkup mark;
    uint64 childID;
    uint64 tid;

    // const struct SpatialIndex::QuadNode &indexNode = index_->nodes_[id];
    const struct SpatialIndex::QuadNode *indexNode = &index_->nodes_[id];

    //
    // do the face test on the triangle

    // was: mark =  testNode(V(NV(0)),V(NV(1)),V(NV(2)));
    // changed to by Gyorgy Fekete. Overall Speedup approx. 2%

    mark = testNode(id); // was:(indexNode or  id);

    switch (mark)
    {
        case fULL:
            tid = N(id).id_;

            saveTrixel(tid); //was:  plist_->push_back(tid);

            return mark;
        case rEJECT:
            tid = N(id).id_;
            return mark;
        default:
            // if pARTIAL or dONTKNOW, then continue, test children,
            //    but do not reach beyond the leaf nodes.
            //    If Convex is fully contained within one (sWALLOWED),
            //    we can stop looking further in another child

            // #define NC(n,m)	index_->nodes_[(n)].childID_[(m)]	// the children n->m
            // childID = index_->nodes_[id].childID_[0];

            // Test how many of the four children are rejected?
            //
            // [ed:algo]
            //
            break;
    }

    // NEW NEW algorithm  Disabled when enablenew is 0
    //
    {
        childID = indexNode->childID_[0];
        if (childID != 0)
        {
            ////////////// [ed:split]
            tid     = N(id).id_;
            childID = indexNode->childID_[0];
            testTrixel(childID);
            childID = indexNode->childID_[1];
            testTrixel(childID);
            childID = indexNode->childID_[2];
            testTrixel(childID);
            childID = indexNode->childID_[3];
            testTrixel(childID);
        }
        else /// No children...
        {
            if (addlevel_)
            {
                testPartial(addlevel_, N(id).id_, V(NV(0)), V(NV(1)), V(NV(2)), 0);
            }
            else
            {
                saveTrixel(N(id).id_); // was: plist_->push_back(N(id).id_);
            }
        }
    } ///////////////////////////// END OF NEW ALGORITHM returns mark below;

    /* NEW NEW NEW
       If rejected, then we return [done]
       If full, then we list the id (propagate to children) [done]

       If partial, then we look ahead to see how many children are rejected.
       But ah, next iteration could benefit from having computed this already.

       If two children are rejected, then we stop
       If one or 0 nodes are rejected, then we
    */
    return mark;
}

/////////////TESTPARTIAL//////////////////////////////////
// testPartial: test a triangle's subtriangle whether they are partial.
// if level is nonzero, recurse.
//
void RangeConvex::testPartial(size_t level, uint64 id, const SpatialVector &v0, const SpatialVector &v1,
                              const SpatialVector &v2, int PPrev)
{
    uint64 ids[4], id0;
    SpatialMarkup m[4];
    int P, F; // count number of partials and fulls

    SpatialVector w0 = v1 + v2;
    w0.normalize();
    SpatialVector w1 = v0 + v2;
    w1.normalize();
    SpatialVector w2 = v1 + v0;
    w2.normalize();

    ids[0] = id0 = id << 2;
    ids[1]       = id0 + 1;
    ids[2]       = id0 + 2;
    ids[3]       = id0 + 3;

    m[0] = testNode(v0, w2, w1);
    m[1] = testNode(v1, w0, w2);
    m[2] = testNode(v2, w1, w0);
    m[3] = testNode(w0, w1, w2);

    F = (m[0] == fULL) + (m[1] == fULL) + (m[2] == fULL) + (m[3] == fULL);
    P = (m[0] == pARTIAL) + (m[1] == pARTIAL) + (m[2] == pARTIAL) + (m[3] == pARTIAL);

    //
    // Several interesting cases for saving this (the parent) trixel.
    // Case P==4, all four children are partials, so pretend parent is full, we save and return
    // Case P==3, and F==1, most of the parent is in, so pretend that parent is full again
    // Case P==2 or 3, but the previous testPartial had three partials, so parent was in an arc
    // as opposed to previous partials being fewer, so parent was in a tiny corner...

    if ((level-- <= 0) || ((P == 4) || (F >= 2) || (P == 3 && F == 1) || (P > 1 && PPrev == 3)))
    {
        saveTrixel(id);
        return;
    }
    else
    {
        // look at each child, see if some need saving;
        for (int i = 0; i < 4; i++)
        {
            if (m[i] == fULL)
            {
                saveTrixel(ids[i]);
            }
        }
        // look at the four kids again, for partials

        if (m[0] == pARTIAL)
            testPartial(level, ids[0], v0, w2, w1, P);
        if (m[1] == pARTIAL)
            testPartial(level, ids[1], v1, w0, w2, P);
        if (m[2] == pARTIAL)
            testPartial(level, ids[2], v2, w1, w0, P);
        if (m[3] == pARTIAL)
            testPartial(level, ids[3], w0, w1, w2, P);
    }
}

/////////////TESTNODE/////////////////////////////////////
// testNode: tests the QuadNodes for intersections.
//
SpatialMarkup RangeConvex::testNode(uint64 id)
//uint64 id)
// const struct SpatialIndex::QuadNode *indexNode)
{
    const SpatialVector *v0, *v1, *v2;
    // const struct SpatialIndex::QuadNode &indexNode = index_->nodes_[id];
    const struct SpatialIndex::QuadNode *indexNode = &index_->nodes_[id];
    int m;
    m  = indexNode->v_[0];
    v0 = &index_->vertices_[m]; // the vertex vector m

    m  = indexNode->v_[1];
    v1 = &index_->vertices_[m];

    m  = indexNode->v_[2];
    v2 = &index_->vertices_[m];

    // Start with testing the vertices for the QuadNode with this convex.
    int vsum = testVertex(v0) + testVertex(v1) + testVertex(v2);

    SpatialMarkup mark = testTriangle(*v0, *v1, *v2, vsum);

    // since we cannot play games using the on-the-fly triangles,
    // substitute dontknow with partial.
    if (mark == dONTKNOW)
        mark = pARTIAL;

    return mark;
}
SpatialMarkup RangeConvex::testNode(const SpatialVector &v0, const SpatialVector &v1, const SpatialVector &v2)
{
    // Start with testing the vertices for the QuadNode with this convex.

    int vsum = testVertex(v0) + testVertex(v1) + testVertex(v2);

    SpatialMarkup mark = testTriangle(v0, v1, v2, vsum);

    // since we cannot play games using the on-the-fly triangles,
    // substitute dontknow with partial.
    if (mark == dONTKNOW)
        mark = pARTIAL;

    return mark;
}

/////////////TESTTRIANGLE//////////////////////////////////
// testTriangle: tests a triangle given by 3 vertices if
// it intersects the convex.
//
SpatialMarkup RangeConvex::testTriangle(const SpatialVector &v0, const SpatialVector &v1, const SpatialVector &v2,
                                        int vsum)
{
    if (vsum == 1 || vsum == 2)
        return pARTIAL;

    // If vsum = 3 then we have all vertices inside the convex.
    // Now use the following decision tree:
    //
    // * If the sign of the convex is pOS or zERO : mark as fULL intersection.
    //
    // * Else, test for holes inside the triangle. A 'hole' is a nEG constraint
    //   that has its center inside the triangle. If there is such a hole,
    //   return pARTIAL intersection.
    //
    // * Else (no holes, sign nEG or mIXED) test for intersection of nEG
    //   constraints with the edges of the triangle. If there are such,
    //   return pARTIAL intersection.
    //
    // * Else return fULL intersection.

    if (vsum == 3)
    {
        if (sign_ == pOS || sign_ == zERO)
            return fULL;
        if (testHole(v0, v1, v2))
            return pARTIAL;
        if (testEdge(v0, v1, v2))
            return pARTIAL;
        return fULL;
    }

    // If we have reached that far, we have vsum=0. There is no definite
    // decision making possible here with our methods, the markup may result
    // in dONTKNOW. The decision tree is the following:
    //
    // * Test with bounding circle of the triangle.
    //
    //   # If the sign of the convex zERO test with the precalculated
    //     bounding circle of the convex. If it does not intersect with the
    //     triangle's bounding circle, rEJECT.
    //
    //   # If the sign of the convex is nonZERO: if the bounding circle
    //     lies outside of one of the constraints, rEJECT.
    //
    // * Else: there was an intersection with the bounding circle.
    //
    //   # For zERO convexes, test whether the convex intersects the edges.
    //     If none of the edges of the convex intersects with the edges of
    //     the triangle, we have a rEJECT. Else, pARTIAL.
    //
    //   # If sign of convex is pOS, or miXED and the smallest constraint does
    //     not intersect the edges and has its center inside the triangle,
    //     return sWALLOW. If no intersection of edges and center outside
    //     triangle, return rEJECT.
    //
    //   # So the smallest constraint DOES intersect with the edges. If
    //     there is another pOS constraint which does not intersect with
    //     the edges, and has its center outside the triangle, return
    //     rEJECT. If its center is inside the triangle return sWALLOW.
    //     Else, return pARTIAL for pOS and dONTKNOW for mIXED signs.
    //
    // * If we are here, return dONTKNOW. There is an intersection with
    //   the bounding circle, none of the vertices is inside the convex and
    //   we have very strange possibilities left for pOS and mIXED signs. For
    //   nEG, i.e. all constraints negative, we also have some complicated
    //   things left for which we cannot test further.

    if (!testBoundingCircle(v0, v1, v2))
        return rEJECT;

    if (sign_ == pOS || sign_ == mIXED || (sign_ == zERO && constraints_.size() <= 2))
    {
        // Does the smallest constraint intersect with the edges?
        if (testEdgeConstraint(v0, v1, v2, 0))
        {
            // Is there another positive constraint that does NOT intersect with
            // the edges?
            size_t cIndex;
            if ((cIndex = testOtherPosNone(v0, v1, v2)))
            {
                // Does that constraint lie inside or outside of the triangle?
                if (testConstraintInside(v0, v1, v2, cIndex))
                    return pARTIAL;
                // Does the triangle lie completely within that constr?
                else if (constraints_[cIndex].contains(v0))
                    return pARTIAL;
                else
                    return rEJECT;
            }
            else
            {
                if (sign_ == pOS || sign_ == zERO)
                    return pARTIAL;
                else
                    return dONTKNOW;
            }
        }
        else
        {
            if (sign_ == pOS || sign_ == zERO)
            {
                // Does the smallest lie inside or outside the triangle?
                if (testConstraintInside(v0, v1, v2, 0))
                    return pARTIAL;
                else
                    return rEJECT;
            }
            else
                return dONTKNOW;
        }
    }
    else if (sign_ == zERO)
    {
        if (corners_.size() > 0 && testEdge0(v0, v1, v2))
            return pARTIAL;
        else
            return rEJECT;
    }
    return pARTIAL;
}

/////////////TESTVERTEX/////////////////////////////////////
// testVertex: same as above, but for any spatialvector, no markup speedup
//
int RangeConvex::testVertex(const SpatialVector &v)
{
    for (size_t i = 0; i < constraints_.size(); i++)
        if ((constraints_[i].a_ * v) < constraints_[i].d_)
            return 0;

    return 1;
}
int RangeConvex::testVertex(const SpatialVector *v)
{
    for (size_t i = 0; i < constraints_.size(); i++)
        if ((constraints_[i].a_ * *v) < constraints_[i].d_)
            return 0;

    return 1;
}

/////////////TESTHOLE/////////////////////////////////////
// testHole: test for holes. If there is a negative constraint whose center
//           is inside the triangle, we speak of a hole. Returns true if
//	     found one.
//
bool RangeConvex::testHole(const SpatialVector &v0, const SpatialVector &v1, const SpatialVector &v2)
{
    bool test = false;

    for (size_t i = 0; i < constraints_.size(); i++)
    {
        if (constraints_[i].sign_ == nEG) // test only 'holes'
        {
            // If (a ^ b * c) < 0, vectors abc point clockwise.
            // -> center c not inside triangle, since vertices a,b are ordered
            // counter-clockwise. The comparison here is the other way
            // round because c points into the opposite direction as the hole

            if (((v0 ^ v1) * constraints_[i].a_) > 0.0L)
                continue;
            if (((v1 ^ v2) * constraints_[i].a_) > 0.0L)
                continue;
            if (((v2 ^ v0) * constraints_[i].a_) > 0.0L)
                continue;
            test = true;
            break;
        }
    }
    return test;
}

/////////////TESTEDGE0////////////////////////////////////
// testEdge0: test if the edges intersect with the zERO convex.
//            The edges are given by the vertex vectors e[0-2]
//	      All constraints are great circles, so test if their intersect
//            with the edges is inside or outside the convex.
//            If any edge intersection is inside the convex, return true.
//            If all edge intersections are outside, check whether one of
//            the corners is inside the triangle. If yes, all of them must be
//            inside -> return true.
//
bool RangeConvex::testEdge0(const SpatialVector &v0, const SpatialVector &v1, const SpatialVector &v2)
{
    // We have constructed the corners_ array in a certain direction.
    // now we can run around the convex, check each side against the 3
    // triangle edges. If any of the sides has its intersection INSIDE
    // the side, return true. At the end test if a corner lies inside
    // (because if we get there, none of the edges intersect, but it
    // can be that the convex is fully inside the triangle. so to test
    // one single edge is enough)

    struct edgeStruct
    {
        SpatialVector e;         // The half-sphere this edge delimits
        float64 l;               // length of edge
        const SpatialVector *e1; // first end
        const SpatialVector *e2; // second end
    } edge[3];

    // fill the edge structure for each side of this triangle
    edge[0].e  = v0 ^ v1;
    edge[0].e1 = &v0;
    edge[0].e2 = &v1;
    edge[1].e  = v1 ^ v2;
    edge[1].e1 = &v1;
    edge[1].e2 = &v2;
    edge[2].e  = v2 ^ v0;
    edge[2].e1 = &v2;
    edge[2].e2 = &v0;
    edge[0].l  = acos(v0 * v1);
    edge[1].l  = acos(v1 * v2);
    edge[2].l  = acos(v2 * v0);

    for (size_t i = 0; i < corners_.size(); i++)
    {
        size_t j = 0;
        if (i < corners_.size() - 1)
            j = i + 1;
        SpatialVector a1;
        float64 l1, l2;                                     // lengths of the arcs from intersection to edge corners
        float64 cedgelen = acos(corners_[i] * corners_[j]); // length of edge of convex

        // calculate the intersection - all 3 edges
        for (size_t iedge = 0; iedge < 3; iedge++)
        {
            a1 = (edge[iedge].e) ^ (corners_[i] ^ corners_[j]);
            a1.normalize();
            // if the intersection a1 is inside the edge of the convex,
            // its distance to the corners is smaller than the edgelength.
            // this test has to be done for both the edge of the convex and
            // the edge of the triangle.
            for (size_t k = 0; k < 2; k++)
            {
                l1 = acos(corners_[i] * a1);
                l2 = acos(corners_[j] * a1);
                if (l1 - cedgelen <= gEpsilon && l2 - cedgelen <= gEpsilon)
                {
                    l1 = acos(*(edge[iedge].e1) * a1);
                    l2 = acos(*(edge[iedge].e2) * a1);
                    if (l1 - edge[iedge].l <= gEpsilon && l2 - edge[iedge].l <= gEpsilon)
                        return true;
                }
                a1 *= -1.0; // do the same for the other intersection
            }
        }
    }
    return testVectorInside(v0, v1, v2, corners_[0]);
}

/////////////TESTEDGE/////////////////////////////////////
// testEdge: test if edges intersect with constraint. This problem
//           is solved by a quadratic equation. Return true if there is
//	     an intersection.
//
bool RangeConvex::testEdge(const SpatialVector &v0, const SpatialVector &v1, const SpatialVector &v2)
{
    for (size_t i = 0; i < constraints_.size(); i++)
    {
        if (constraints_[i].sign_ == nEG) // test only 'holes'
        {
            if (eSolve(v0, v1, i))
                return true;
            if (eSolve(v1, v2, i))
                return true;
            if (eSolve(v2, v0, i))
                return true;
        }
    }
    return false;
}

/////////////ESOLVE///////////////////////////////////////
// eSolve: solve the quadratic eq. for intersection of an edge with a circle
//         constraint. Edge given by grand circle running through v1, v2
//         Constraint given by cIndex.
bool RangeConvex::eSolve(const SpatialVector &v1, const SpatialVector &v2, size_t cIndex)
{
    float64 gamma1 = v1 * constraints_[cIndex].a_;
    float64 gamma2 = v2 * constraints_[cIndex].a_;
    float64 mu     = v1 * v2;
    float64 u2     = (1 - mu) / (1 + mu);

    float64 a = -u2 * (gamma1 + constraints_[cIndex].d_);
    float64 b = gamma1 * (u2 - 1) + gamma2 * (u2 + 1);
    float64 c = gamma1 - constraints_[cIndex].d_;

    float64 D = b * b - 4 * a * c;

    if (D < 0.0L)
        return false; // no intersection

    // calculate roots a'la Numerical Recipes

    float64 q = -0.5L * (b + (SGN(b) * sqrt(D)));

    float64 root1(0), root2(0);
    int i = 0;

    if (a > gEpsilon || a < -gEpsilon)
    {
        root1 = q / a;
        i++;
    }
    if (q > gEpsilon || q < -gEpsilon)
    {
        root2 = c / q;
        i++;
    }

    // Check whether the roots lie within [0,1]. If not, the intersection
    // is outside the edge.

    if (i == 0)
        return false; // no solution
    if (root1 >= 0.0L && root1 <= 1.0L)
        return true;
    if (i == 2 && ((root1 >= 0.0L && root1 <= 1.0L) || (root2 >= 0.0L && root2 <= 1.0L)))
        return true;

    return false;
}

/////////////TESTBOUNDINGCIRCLE///////////////////////////
// testBoundingCircle: test for boundingCircles intersecting with constraint
//
bool RangeConvex::testBoundingCircle(const SpatialVector &v0, const SpatialVector &v1, const SpatialVector &v2)
{
    // Set the correct direction: The normal vector to the triangle plane
    SpatialVector c = (v1 - v0) ^ (v2 - v1);
    c.normalize();

    // Set the correct opening angle: Since the plane cutting out the triangle
    // also correctly cuts out the bounding cap of the triangle on the sphere,
    // we can take any corner to calculate the opening angle
    float64 d = acos(c * v0);

    // for zero convexes, we have calculated a bounding circle for the convex.
    // only test with this single bounding circle.

    if (sign_ == zERO)
    {
        float64 tst;
        if (((tst = c * boundingCircle_.a_) < -1.0L + gEpsilon ? gPi : acos(tst)) > (d + boundingCircle_.s_))
            return false;
        return true;
    }

    // for all other convexes, test every constraint. If the bounding
    // circle lies completely outside of one of the constraints, reject.
    // else, accept.

    size_t i;
    for (i = 0; i < constraints_.size(); i++)
    {
        if (((c * constraints_[i].a_) < -1.0L + gEpsilon ? gPi : acos(c * constraints_[i].a_)) >
            (d + constraints_[i].s_))
            return false;
    }
    return true;
}

/////////////TESTEDGECONSTRAINT///////////////////////////
// testEdgeConstraint: test if edges intersect with a given constraint.
//
bool RangeConvex::testEdgeConstraint(const SpatialVector &v0, const SpatialVector &v1, const SpatialVector &v2,
                                     size_t cIndex)
{
    if (eSolve(v0, v1, cIndex))
        return true;
    if (eSolve(v1, v2, cIndex))
        return true;
    if (eSolve(v2, v0, cIndex))
        return true;
    return false;
}

/////////////TESTOTHERPOSNONE/////////////////////////////
// testOtherPosNone: test for other positive constraints that do
//                   not intersect with an edge. Return its index
//
size_t RangeConvex::testOtherPosNone(const SpatialVector &v0, const SpatialVector &v1, const SpatialVector &v2)
{
    size_t i = 1;
    while (i < constraints_.size() && constraints_[i].sign_ == pOS)
    {
        if (!testEdgeConstraint(v0, v1, v2, i))
            return i;
        i++;
    }
    return 0;
}

/////////////TESTCONSTRAINTINSIDE/////////////////////////
// testConstraintInside: look if a constraint is inside the triangle
//
bool RangeConvex::testConstraintInside(const SpatialVector &v0, const SpatialVector &v1, const SpatialVector &v2,
                                       size_t i)
{
    return testVectorInside(v0, v1, v2, constraints_[i].a_);
}

/////////////TESTVECTORINSIDE////////////////////////////
// testVectorInside: look if a vector is inside the triangle
//
bool RangeConvex::testVectorInside(const SpatialVector &v0, const SpatialVector &v1, const SpatialVector &v2,
                                   SpatialVector &v)
{
    // If (a ^ b * c) < 0, vectors abc point clockwise.
    // -> center c not inside triangle, since vertices are ordered
    // counter-clockwise.

    if ((((v0 ^ v1) * v) < 0) || (((v1 ^ v2) * v) < 0) || (((v2 ^ v0) * v) < 0))
        return false;
    return true;
}
