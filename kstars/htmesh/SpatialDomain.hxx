//#     Filename:       SpatialDomain.hxx
//#
//#     H declaratinos for SpatialDomain
//#
//#
//#     Author:         Peter Z. Kunszt
//#     
//#     Date:           October 16, 1998
//#
//#		Copyright (C) 2000  Peter Z. Kunszt, Alex S. Szalay, Aniruddha R. Thakar
//#                     The Johns Hopkins University
//#

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

/////////////>>///////////////////////////////////////////
// read from istream
//
inline
istream& operator >>( istream& in, SpatialDomain & c) {
  Convexio::read(in, c);
  return(in);
}

/////////////<<///////////////////////////////////////////
// write to ostream
//
inline
ostream& operator <<( ostream& out, const SpatialDomain & c) {
  c.write(out);
  return(out);
}

extern  int compUint64(const void*, const void*);
extern  int compRange (const void*, const void*);
