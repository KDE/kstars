//#     Filename:       SpatialIndex.hxx
//#
//#     H Implementations for spatialindex
//#
//#     Author:         Peter Z. Kunszt, based on A. Szalay s code
//#
//#     Date:           October 15, 1998
//#
//#		Copyright (C) 2000  Peter Z. Kunszt, Alex S. Szalay, Aniruddha R. Thakar
//#                     The Johns Hopkins University
//#

//////////////////IDBYPOINT////////////////////////////////////////////////
// Find a leaf node where a ra/dec points to
inline uint64
SpatialIndex::idByPoint(const float64 & ra, const float64 & dec) const {
  SpatialVector v(ra,dec);
  return idByPoint(v);
}
