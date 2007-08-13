//#     Filename:       SpatialEdge.cpp
//#
//#     The SpatialEdge class is defined here.
//#
//#     Author:         Peter Z. Kunszt based on A. Szalay's code
//#     
//#     Date:           October 15, 1998
//#
//#		Copyright (C) 2000  Peter Z. Kunszt, Alex S. Szalay, Aniruddha R. Thakar
//#                     The Johns Hopkins University
//#
//#     Modification History:
//#
//#     Oct 18, 2001 : Dennis C. Dinge -- Replaced ValVec with std::vector
//#

#include "SpatialEdge.h"

// ===========================================================================
//
// Macro definitions for readability
//
// ===========================================================================
#define V(x) tree_.vertices_[tree_.nodes_[index].v_[(x)]]
#define IV(x) tree_.nodes_[index].v_[(x)]
#define W(x) tree_.vertices_[tree_.nodes_[index].w_[(x)]]
#define IW(x) tree_.nodes_[index].w_[(x)]
#define LAYER tree_.layers_[layerindex_]


// ===========================================================================
//
// Member functions for class SpatialEdge
//
// ===========================================================================

/////////////CONSTRUCTOR//////////////////////////////////
//
SpatialEdge::SpatialEdge(SpatialIndex & tree, size_t layerindex) :
  tree_(tree), layerindex_(layerindex) {

  edges_ = new Edge  [LAYER.nEdge_ + 1];
  lTab_  = new Edge* [LAYER.nVert_ * 6];

  // initialize lookup table, we depend on that NULL
  for(size_t i = 0; i < LAYER.nVert_ * 6; i++)
    lTab_[i] = NULL;

  // first vertex index for the vertices to be generated
  index_ = LAYER.nVert_;
}


/////////////DESTRUCTOR///////////////////////////////////
//
SpatialEdge::~SpatialEdge() {
  delete[] edges_;
  delete[] lTab_;
}

/////////////MAKEMIDPOINTS////////////////////////////////
// makeMidPoints: interface to this class. Set midpoints of every
//                node in this layer.
void
SpatialEdge::makeMidPoints()
{
  size_t c=0;
  size_t index;

  // build up the new edges 

  index = (size_t)LAYER.firstIndex_;
  for(size_t i=0; i < LAYER.nNode_; i++,index++){
    c = newEdge(c,index,0);
    c = newEdge(c,index,1);
    c = newEdge(c,index,2);
  }
}


/////////////NEWEDGE//////////////////////////////////////
// newEdge: determines whether the edge em is already in the list.  k
//          is the label of the edge within the node Returns index of next
//          edge, if not found, or returns same if it is already there.  Also
//          registers the midpoint in the node.

size_t
SpatialEdge::newEdge(size_t emindex, size_t index, int k)
{
  Edge *en, *em;
  size_t swap;

  em = &edges_[emindex];

  switch (k) {
  case 0:
    em->start_ = IV(1);
    em->end_   = IV(2);
    break;
  case 1:
    em->start_ = IV(0);
    em->end_   = IV(2);
    break;
  case 2:
    em->start_ = IV(0);
    em->end_   = IV(1);
    break;
  }

  // sort the vertices by increasing index

  if(em->start_ > em->end_) {
    swap = em->start_;
    em->start_ = em->end_;
    em->end_ = swap;
  }

  // check all previous edges for a match, return pointer if 
  // already present, log the midpoint with the new face as well
   
  if( (en=edgeMatch(em)) != NULL){
    IW(k) = en->mid_;
    return emindex;
  }

// this is a new edge, immediately process the midpoint, 
// and save it with the nodes and the edge as well

  insertLookup(em);
  IW(k)      = getMidPoint(em);
  em->mid_   = IW(k);
  return ++emindex;
}


/////////////INSERTLOOKUP/////////////////////////////////
// insertLookup: insert the edge em into the lookup table.
//               indexed by em->start_.
//               Every vertex has at most 6 edges, so only
//               that much lookup needs to be done.
void 
SpatialEdge::insertLookup(Edge *em)
{
int j = 6 * em->start_;
int i;

// do not loop beyond 6

   for(i=0; i<6; i++, j++)
       if ( lTab_[j] == NULL ) {
           lTab_[j] = em;
           return;
       }
}

/////////////EDGEMATCH////////////////////////////////////
// edgeMatch: fast lookup using the first index em->start_.
//            return pointer to edge if matches, null if not.
#if defined(__sun) && !defined(__gnu)
Edge *
#else
SpatialEdge::Edge *
#endif
SpatialEdge::edgeMatch(Edge *em)
{
int i = 6 * em->start_;

   while ( lTab_[i] != NULL ) {
     if(em->end_ == lTab_[i]->end_ ) return lTab_[i];
     i++;
   }
   return NULL;
}

/////////////GETMIDPOINT//////////////////////////////////
// getMidPoint: compute the midpoint of the edge using vector
//              algebra and return its index in the vertex list
size_t 
SpatialEdge::getMidPoint(Edge *em)
{
  tree_.vertices_[index_] = tree_.vertices_[em->start_] + 
                                  tree_.vertices_[em->end_]; 
  tree_.vertices_[index_].normalize();
  return index_++;
}
