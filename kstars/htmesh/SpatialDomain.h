#ifndef _SpatialDomain_h
#define _SpatialDomain_h

//#     Filename:       SpatialDomain.h
//#
//#     Classes defined here: SpatialDomain
//#
//#
//#     Author:         Peter Z. Kunszt
//#     
//#     Date:           October 16, 1998
//#
//#		Copyright (C) 2000  Peter Z. Kunszt, Alex S. Szalay, Aniruddha R. Thakar
//#                     The Johns Hopkins University
//#
//#     Modification History:
//#
//#     Oct 18, 2001 : Dennis C. Dinge -- Replaced ValVec with std::vector
//#

#include "Htmio.h"
#include "RangeConvex.h"
#include <HtmRange.h>
#include <vector>

//########################################################################
//
// Spatial Domain class
//
// 

/** A spatial domain is a list of spatial convexes. So we can have
 really disjoint pieces of the sky defined by a domain.  */

class LINKAGE SpatialDomain {
public:
  /// Constructor
  SpatialDomain(const SpatialIndex * idx = 0);

  /// Destructor
  ~SpatialDomain();

  /// Set index pointer
  void setIndex(const SpatialIndex *);

  /// Add a convex
  void add(RangeConvex &rc);

  /// Simplify the Domain, remove redundancies
  void simplify();

  /// intersection, returns ranges of HTMids, or individual HTMids is valren is true
  bool intersect(const SpatialIndex * idx, HtmRange *htmrange, bool varlen);

  /// numConvexes: give back the number of convexes
  size_t numConvexes();

  /// [] operator: give back convex
  RangeConvex & operator [](size_t i);

  /// write to stream
  void write(std::ostream&) const;

  const SpatialIndex *index; 		/// A pointer to the index

  static void ignoreCrLf(std::istream &);

  void setOlevel(int level);
  int getOlevel(void){return olevel;};

protected:
  int olevel;
  std::vector<RangeConvex> convexes_;      /// The vector of convexes

public:
  static uint64 topBit_;
};

// #include "SpatialDomain.hxx"


inline
RangeConvex &
SpatialDomain::operator [](size_t i) {
  return convexes_[i];
}

inline
size_t
SpatialDomain::numConvexes() {
  return convexes_.size();
}

LINKAGE std::istream& operator >>( std::istream& in, SpatialDomain & c);
LINKAGE std::ostream& operator <<( std::ostream& out, const SpatialDomain & c);
LINKAGE std::ostream& operator <<( std::ostream& out, SpatialDomain & c);

extern  int compUint64(const void*, const void*);
extern  int compRange (const void*, const void*);

#endif
