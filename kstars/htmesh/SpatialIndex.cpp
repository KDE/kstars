//#     Filename:       SpatialIndex.cpp
//#
//#     The SpatialIndex class is defined here.
//#
//#     Author:         Peter Z. Kunszt based on A. Szalay's code
//#
//#     Date:           October 15, 1998
//#
//#	    SPDX-FileCopyrightText: 2000 Peter Z. Kunszt Alex S. Szalay, Aniruddha R. Thakar
//#                     The Johns Hopkins University
//#
//#     Modification History:
//#
//#     Oct 18, 2001 : Dennis C. Dinge -- Replaced ValVec with std::vector
//#		Jul 25, 2002 : Gyorgy Fekete -- Added pointById()
//#

#include "SpatialIndex.h"
#include "SpatialException.h"

#ifdef _WIN32
#include <malloc.h>
#else
#ifdef __APPLE__
#include <sys/malloc.h>
#else
#include <cstdlib>
#endif
#endif

#include <cstdio>
#include <cmath>
// ===========================================================================
//
// Macro definitions for readability
//
// ===========================================================================

#define N(x)      nodes_[(x)]
#define V(x)      vertices_[nodes_[index].v_[(x)]]
#define IV(x)     nodes_[index].v_[(x)]
#define W(x)      vertices_[nodes_[index].w_[(x)]]
#define IW(x)     nodes_[index].w_[(x)]
#define ICHILD(x) nodes_[index].childID_[(x)]

#define IV_(x)     nodes_[index_].v_[(x)]
#define IW_(x)     nodes_[index_].w_[(x)]
#define ICHILD_(x) nodes_[index_].childID_[(x)]
#define IOFFSET    9

// ===========================================================================
//
// Member functions for class SpatialIndex
//
// ===========================================================================

/////////////CONSTRUCTOR//////////////////////////////////
//
SpatialIndex::SpatialIndex(size_t maxlevel, size_t buildlevel)
    : maxlevel_(maxlevel), buildlevel_((buildlevel == 0 || buildlevel > maxlevel) ? maxlevel : buildlevel)
{
    size_t nodes, vertices;

    vMax(&nodes, &vertices);
    layers_.resize(buildlevel_ + 1); // allocate enough space already
    nodes_.resize(nodes + 1);        // allocate space for all nodes
    vertices_.resize(vertices + 1);  // allocate space for all vertices

    N(0).index_ = 0; // initialize invalid node

    // initialize first layer
    layers_[0].level_       = 0;
    layers_[0].nVert_       = 6;
    layers_[0].nNode_       = 8;
    layers_[0].nEdge_       = 12;
    layers_[0].firstIndex_  = 1;
    layers_[0].firstVertex_ = 0;

    // set the first 6 vertices
    float64 v[6][3] = {
        { 0.0L, 0.0L, 1.0L },  // 0
        { 1.0L, 0.0L, 0.0L },  // 1
        { 0.0L, 1.0L, 0.0L },  // 2
        { -1.0L, 0.0L, 0.0L }, // 3
        { 0.0L, -1.0L, 0.0L }, // 4
        { 0.0L, 0.0L, -1.0L }  // 5
    };

    for (int i = 0; i < 6; i++)
        vertices_[i].set(v[i][0], v[i][1], v[i][2]);

    // create the first 8 nodes - index 1 through 8
    index_ = 1;
    newNode(1, 5, 2, 8, 0);  // S0
    newNode(2, 5, 3, 9, 0);  // S1
    newNode(3, 5, 4, 10, 0); // S2
    newNode(4, 5, 1, 11, 0); // S3
    newNode(1, 0, 4, 12, 0); // N0
    newNode(4, 0, 3, 13, 0); // N1
    newNode(3, 0, 2, 14, 0); // N2
    newNode(2, 0, 1, 15, 0); // N3

    //  loop through buildlevel steps, and build the nodes for each layer

    size_t pl    = 0;
    size_t level = buildlevel_;
    while (level-- > 0)
    {
        SpatialEdge edge(*this, pl);
        edge.makeMidPoints();
        makeNewLayer(pl);
        ++pl;
    }

    sortIndex();
}

/////////////NODEVERTEX///////////////////////////////////
// nodeVertex: return the vectors of the vertices, based on the ID
//
void SpatialIndex::nodeVertex(const uint64 id, SpatialVector &v0, SpatialVector &v1, SpatialVector &v2) const
{
    if (buildlevel_ == maxlevel_)
    {
        uint32 idx = (uint32)id - leaves_ + IOFFSET; // -jbb: Fix segfault.  See "idx =" below.
        v0         = vertices_[nodes_[idx].v_[0]];
        v1         = vertices_[nodes_[idx].v_[1]];
        v2         = vertices_[nodes_[idx].v_[2]];
        return;
    }

    // buildlevel < maxlevel
    // get the id of the stored leaf that we are in
    // and get the vertices of the node we want
    uint64 sid = id >> ((maxlevel_ - buildlevel_) * 2);
    uint32 idx = (uint32)(sid - storedleaves_ + IOFFSET);

    v0 = vertices_[nodes_[idx].v_[0]];
    v1 = vertices_[nodes_[idx].v_[1]];
    v2 = vertices_[nodes_[idx].v_[2]];

    // loop through additional levels,
    // pick the correct triangle accordingly, storing the
    // vertices in v1,v2,v3
    for (uint32 i = buildlevel_ + 1; i <= maxlevel_; i++)
    {
        uint64 j         = (id >> ((maxlevel_ - i) * 2)) & 3;
        SpatialVector w0 = v1 + v2;
        w0.normalize();
        SpatialVector w1 = v0 + v2;
        w1.normalize();
        SpatialVector w2 = v1 + v0;
        w2.normalize();

        switch (j)
        {
            case 0:
                v1 = w2;
                v2 = w1;
                break;
            case 1:
                v0 = v1;
                v1 = w0;
                v2 = w2;
                break;
            case 2:
                v0 = v2;
                v1 = w1;
                v2 = w0;
                break;
            case 3:
                v0 = w0;
                v1 = w1;
                v2 = w2;
                break;
        }
    }
}

/////////////MAKENEWLAYER/////////////////////////////////
// makeNewLayer: generate a new layer and the nodes in it
//
void SpatialIndex::makeNewLayer(size_t oldlayer)
{
    uint64 index, id;
    size_t newlayer                = oldlayer + 1;
    layers_[newlayer].level_       = layers_[oldlayer].level_ + 1;
    layers_[newlayer].nVert_       = layers_[oldlayer].nVert_ + layers_[oldlayer].nEdge_;
    layers_[newlayer].nNode_       = 4 * layers_[oldlayer].nNode_;
    layers_[newlayer].nEdge_       = layers_[newlayer].nNode_ + layers_[newlayer].nVert_ - 2;
    layers_[newlayer].firstIndex_  = index_;
    layers_[newlayer].firstVertex_ = layers_[oldlayer].firstVertex_ + layers_[oldlayer].nVert_;

    uint64 ioffset = layers_[oldlayer].firstIndex_;

    for (index = ioffset; index < ioffset + layers_[oldlayer].nNode_; index++)
    {
        id        = N(index).id_ << 2;
        ICHILD(0) = newNode(IV(0), IW(2), IW(1), id++, index);
        ICHILD(1) = newNode(IV(1), IW(0), IW(2), id++, index);
        ICHILD(2) = newNode(IV(2), IW(1), IW(0), id++, index);
        ICHILD(3) = newNode(IW(0), IW(1), IW(2), id, index);
    }
}

/////////////NEWNODE//////////////////////////////////////
// newNode: make a new node
//
uint64 SpatialIndex::newNode(size_t v1, size_t v2, size_t v3, uint64 id, uint64 parent)
{
    IV_(0) = v1; // vertex indices
    IV_(1) = v2;
    IV_(2) = v3;

    IW_(0) = 0; // middle point indices
    IW_(1) = 0;
    IW_(2) = 0;

    ICHILD_(0) = 0; // child indices
    ICHILD_(1) = 0; // index 0 is invalid node.
    ICHILD_(2) = 0;
    ICHILD_(3) = 0;

    N(index_).id_     = id;     // set the id
    N(index_).index_  = index_; // set the index
    N(index_).parent_ = parent; // set the parent

    return index_++;
}

/////////////VMAX/////////////////////////////////////////
// vMax: compute the maximum number of vertices for the
//       polyhedron after buildlevel of subdivisions and
//       the total number of nodes that we store
//       also, calculate the number of leaf nodes that we eventually have.
//
void SpatialIndex::vMax(size_t *nodes, size_t *vertices)
{
    uint64 nv = 6; // initial values
    uint64 ne = 12;
    uint64 nf = 8;
    int32 i   = buildlevel_;
    *nodes    = (size_t)nf;

    while (i-- > 0)
    {
        nv += ne;
        nf *= 4;
        ne = nf + nv - 2;
        *nodes += (size_t)nf;
    }
    *vertices     = (size_t)nv;
    storedleaves_ = nf;

    // calculate number of leaves
    i = maxlevel_ - buildlevel_;
    while (i-- > 0)
        nf *= 4;
    leaves_ = nf;
}

/////////////SORTINDEX////////////////////////////////////
// sortIndex: sort the index so that the first node is the invalid node
//            (index 0), the next 8 nodes are the root nodes
//            and then we put all the leaf nodes in the following block
//            in ascending id-order.
//            All the rest of the nodes is at the end.
void SpatialIndex::sortIndex()
{
    std::vector<QuadNode> oldnodes(nodes_); // create a copy of the node list
    size_t index;
    size_t nonleaf;
    size_t leaf;

#define ON(x) oldnodes[(x)]

    // now refill the nodes_ list according to our sorting.
    for (index = IOFFSET, leaf = IOFFSET, nonleaf = nodes_.size() - 1; index < nodes_.size(); index++)
    {
        if (ON(index).childID_[0] == 0) // childnode
        {
            // set leaf into list
            N(leaf) = ON(index);
            // set parent's pointer to this leaf
            for (size_t i = 0; i < 4; i++)
            {
                if (N(N(leaf).parent_).childID_[i] == index)
                {
                    N(N(leaf).parent_).childID_[i] = leaf;
                    break;
                }
            }
            leaf++;
        }
        else
        {
            // set nonleaf into list from the end
            // set parent of the children already to this
            // index, they come later in the list.
            N(nonleaf)                         = ON(index);
            ON(N(nonleaf).childID_[0]).parent_ = nonleaf;
            ON(N(nonleaf).childID_[1]).parent_ = nonleaf;
            ON(N(nonleaf).childID_[2]).parent_ = nonleaf;
            ON(N(nonleaf).childID_[3]).parent_ = nonleaf;
            // set parent's pointer to this leaf
            for (size_t i = 0; i < 4; i++)
            {
                if (N(N(nonleaf).parent_).childID_[i] == index)
                {
                    N(N(nonleaf).parent_).childID_[i] = nonleaf;
                    break;
                }
            }
            nonleaf--;
        }
    }
}
//////////////////IDBYNAME/////////////////////////////////////////////////
// Translate ascii leaf name to a uint32
//
// The following encoding is used:
//
// The string leaf name has the always the same structure, it begins with
// an N or S, indicating north or south cap and then numbers 0-3 follow
// indicating which child to descend into. So for a depth-5-index we have
// strings like
//                 N012023  S000222  N102302  etc
//
// Each of the numbers correspond to 2 bits of code (00 01 10 11) in the
// uint32. The first two bits are 10 for S and 11 for N. For example
//
//                 N 0 1 2 0 2 3
//                 11000110001011  =  12683 (dec)
//
// The leading bits are always 0.
//
// --- WARNING: This works only up to 15 levels.
//              (we probably never need more than 7)
//

uint64 SpatialIndex::idByName(const char *name)
{
    uint64 out  = 0, i;
    uint32 size = 0;

    if (name == nullptr) // null pointer-name
        throw SpatialFailure("SpatialIndex:idByName:no name given");
    if (name[0] != 'N' && name[0] != 'S') // invalid name
        throw SpatialFailure("SpatialIndex:idByName:invalid name", name);

    size = strlen(name); // determine string length
    // at least size-2 required, don't exceed max
    if (size < 2)
        throw SpatialFailure("SpatialIndex:idByName:invalid name - too short ", name);
    if (size > HTMNAMEMAX)
        throw SpatialFailure("SpatialIndex:idByName:invalid name - too long ", name);

    for (i = size - 1; i > 0; i--) // set bits starting from the end
    {
        if (name[i] > '3' || name[i] < '0') // invalid name
            throw SpatialFailure("SpatialIndex:idByName:invalid name digit ", name);
        out += (uint64(name[i] - '0') << 2 * (size - i - 1));
    }

    i = 2; // set first pair of bits, first bit always set
    if (name[0] == 'N')
        i++; // for north set second bit too
    out += (i << (2 * size - 2));

    /************************
    // This code may be used later for hashing !
    if(size==2)out -= 8;
    else {
      size -= 2;
      uint32 offset = 0, level4 = 8;
      for(i = size; i > 0; i--) { // calculate 4 ^ (level-1), level = size-2
        offset += level4;
        level4 *= 4;
      }
      out -= level4 - offset;
    }
    **************************/
    return out;
}

//////////////////NAMEBYID/////////////////////////////////////////////////
// Translate uint32 to an ascii leaf name
//
// The encoding described above may be decoded again using the following
// procedure:
//
//  * Traverse the uint32 from left to right.
//  * Find the first 'true' bit.
//  * The first pair gives N (11) or S (10).
//  * The subsequent bit-pairs give the numbers 0-3.
//

char *SpatialIndex::nameById(uint64 id, char *name)
{
    uint32 size = 0, i;
#ifdef _WIN32
    uint64 IDHIGHBIT  = 1;
    uint64 IDHIGHBIT2 = 1;
    IDHIGHBIT         = IDHIGHBIT << 63;
    IDHIGHBIT2        = IDHIGHBIT2 << 62;
#endif

    /*************
    // This code might be useful for hashing later !!

    // calculate the level (i.e. 8*4^level) and add it to the id:
    uint32 level=0, level4=8, offset=8;
    while(id >= offset) {
      if(++level > 13) { ok = false; offset = 0; break; }// level too deep
      level4 *= 4;
      offset += level4;
    }
    id += 2 * level4 - offset;
    **************/

    // determine index of first set bit
    for (i = 0; i < IDSIZE; i += 2)
    {
        if ((id << i) & IDHIGHBIT)
            break;
        if ((id << i) & IDHIGHBIT2) // invalid id
            throw SpatialFailure("SpatialIndex:nameById: invalid ID");
    }
    if (id == 0)
        throw SpatialFailure("SpatialIndex:nameById: invalid ID");

    size = (IDSIZE - i) >> 1;
    // allocate characters
    if (!name)
        name = new char[size + 1];

    // fill characters starting with the last one
    for (i = 0; i < size - 1; i++)
        name[size - i - 1] = '0' + char((id >> i * 2) & 3);

    // put in first character
    if ((id >> (size * 2 - 2)) & 1)
    {
        name[0] = 'N';
    }
    else
    {
        name[0] = 'S';
    }
    name[size] = 0; // end string

    return name;
}
//////////////////POINTBYID////////////////////////////////////////////////
// Find a vector for the leaf node given by its ID
//
void SpatialIndex::pointById(SpatialVector &vec, uint64 ID) const
{
    //	uint64 index;
    float64 center_x, center_y, center_z, sum;
    char name[HTMNAMEMAX];

    SpatialVector v0, v1, v2; //

    this->nodeVertex(ID, v0, v1, v2);

    nameById(ID, name);
    /*
        I started to go this way until I discovered nameByID...
    	Some docs would be nice for this

    	switch(name[1]){
    	case '0':
    		index = name[0] == 'S' ? 1 : 5;
    		break;
    	case '1':
    		index = name[0] == 'S' ? 2 : 6;
    		break;
    	case '2':
    		index = name[0] == 'S' ? 3 : 7;
    		break;
    	case '3':
    		index = name[0] == 'S' ? 4 : 8;
    		break;
    	}
    */
    //    cerr << "---------- Point by id: " << name << endl;
    //	v0.show(); v1.show(); v2.show();
    center_x = v0.x_ + v1.x_ + v2.x_;
    center_y = v0.y_ + v1.y_ + v2.y_;
    center_z = v0.z_ + v1.z_ + v2.z_;
    sum      = center_x * center_x + center_y * center_y + center_z * center_z;
    sum      = sqrt(sum);
    center_x /= sum;
    center_y /= sum;
    center_z /= sum;
    vec.x_ = center_x;
    vec.y_ = center_y;
    vec.z_ = center_z; // I don't want it normalized or radec to be set,
    //	cerr << " - - - - " << endl;
    //	vec.show();
    //	cerr << "---------- Point by id Retuning" << endl;
}
//////////////////IDBYPOINT////////////////////////////////////////////////
// Find a leaf node where a vector points to
//

uint64 SpatialIndex::idByPoint(const SpatialVector &v) const
{
    uint64 index;

    // start with the 8 root triangles, find the one which v points to
    for (index = 1; index <= 8; index++)
    {
        if ((V(0) ^ V(1)) * v < -gEpsilon)
            continue;
        if ((V(1) ^ V(2)) * v < -gEpsilon)
            continue;
        if ((V(2) ^ V(0)) * v < -gEpsilon)
            continue;
        break;
    }
    // loop through matching child until leaves are reached
    while (ICHILD(0) != 0)
    {
        uint64 oldindex = index;
        for (size_t i = 0; i < 4; i++)
        {
            index = nodes_[oldindex].childID_[i];
            if ((V(0) ^ V(1)) * v < -gEpsilon)
                continue;
            if ((V(1) ^ V(2)) * v < -gEpsilon)
                continue;
            if ((V(2) ^ V(0)) * v < -gEpsilon)
                continue;
            break;
        }
    }
    // return if we have reached maxlevel
    if (maxlevel_ == buildlevel_)
        return N(index).id_;

    // from now on, continue to build name dynamically.
    // until maxlevel_ levels depth, continue to append the
    // correct index, build the index on the fly.
    char name[HTMNAMEMAX];
    nameById(N(index).id_, name);
    size_t len       = strlen(name);
    SpatialVector v0 = V(0);
    SpatialVector v1 = V(1);
    SpatialVector v2 = V(2);

    size_t level = maxlevel_ - buildlevel_;
    while (level--)
    {
        SpatialVector w0 = v1 + v2;
        w0.normalize();
        SpatialVector w1 = v0 + v2;
        w1.normalize();
        SpatialVector w2 = v1 + v0;
        w2.normalize();

        if (isInside(v, v0, w2, w1))
        {
            name[len++] = '0';
            v1          = w2;
            v2          = w1;
            continue;
        }
        else if (isInside(v, v1, w0, w2))
        {
            name[len++] = '1';
            v0          = v1;
            v1          = w0;
            v2          = w2;
            continue;
        }
        else if (isInside(v, v2, w1, w0))
        {
            name[len++] = '2';
            v0          = v2;
            v1          = w1;
            v2          = w0;
            continue;
        }
        else if (isInside(v, w0, w1, w2))
        {
            name[len++] = '3';
            v0          = w0;
            v1          = w1;
            v2          = w2;
            continue;
        }
    }
    name[len] = '\0';
    return idByName(name);
}

//////////////////ISINSIDE/////////////////////////////////////////////////
// Test whether a vector is inside a triangle. Input triangle has
// to be sorted in a counter-clockwise direction.
//
bool SpatialIndex::isInside(const SpatialVector &v, const SpatialVector &v0, const SpatialVector &v1,
                            const SpatialVector &v2) const
{
    if ((v0 ^ v1) * v < -gEpsilon)
        return false;
    if ((v1 ^ v2) * v < -gEpsilon)
        return false;
    if ((v2 ^ v0) * v < -gEpsilon)
        return false;
    return true;
}
