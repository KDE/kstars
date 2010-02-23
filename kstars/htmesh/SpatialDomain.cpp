//#     Filename:       SpatialDomain.cpp
//#
//#     The SpatialDomain
//#     classes are defined here.
//#
//#     Author:         Peter Z. Kunszt based on A. Szalay's code
//#     
//#     Date:           October 23, 1998
//#
//#		Copyright (C) 2000  Peter Z. Kunszt, Alex S. Szalay, Aniruddha R. Thakar
//#                     The Johns Hopkins University
//#
//#     Modification History:
//#
//#     Oct 18, 2001 : Dennis C. Dinge -- Replaced ValVec with std::vector
//#

#include "SpatialDomain.h"
#include "SpatialException.h"

// ===========================================================================
//
// Member functions for class SpatialDomain
//
// ===========================================================================

/////////////CONSTRUCTOR//////////////////////////////////
//
// Initialize
//
SpatialDomain::SpatialDomain(const SpatialIndex * i) :
  index(i) {
}

/////////////DESTRUCTOR///////////////////////////////////
//
SpatialDomain::~SpatialDomain()
{
}

/////////////SETINDEX/////////////////////////////////////
//
void
SpatialDomain::setIndex(const SpatialIndex * idx)
{
  index = idx;
}

/////////////ADD//////////////////////////////////////////
//
void
SpatialDomain::add(RangeConvex & c)
{
  convexes_.push_back(c);
  c.setOlevel(olevel);
}

void SpatialDomain::setOlevel(int level)
{
  size_t i;
  this->olevel = level;

  for(i = 0; i < convexes_.size(); i++)  // intersect every convex
    convexes_[i].setOlevel(level);
}

/////////////INTERSECT////////////////////////////////////
//
bool
SpatialDomain::intersect(const SpatialIndex * idx, HtmRange *htmrange, bool varlen)
{

  index = idx;

  size_t i;

  for(i = 0; i < convexes_.size(); i++)  // intersect every convex
    convexes_[i].intersect(index, htmrange, varlen);
  return true;
}

/////////////COMPUINT64///////////////////////////////////
// compare ids
//
int 
compUint64(const void* v1, const void* v2) {
  return (  ( *((uint64 *)v1) < *((uint64 *)v2) ) ? -1 :
	    ( ( *((uint64 *)v1) > *((uint64 *)v2) ) ? 1 : 0 ) );
}

/////////////COMPRANGE///////////////////////////////////
// compare ids
//
int 
compRange(const void* v1, const void* v2) {
  uint64 a = *((uint64 *)v1);
  uint64 b = *((uint64 *)v2);

  while( (a & SpatialDomain::topBit_) == 0 ) a = a << 2 ;
  while( (b & SpatialDomain::topBit_) == 0 ) b = b << 2 ;

  return (  ( a < b ) ? -1 : ( ( a > b ) ? 1 : 0 ) );
}

uint64 SpatialDomain::topBit_ = 0;
