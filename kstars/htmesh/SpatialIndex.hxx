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

/////////////NVERTICES////////////////////////////////////
// nVertices: return number of vertices
inline size_t
SpatialIndex::nVertices() const
{
  //dcdtmp return vertices_.length();
	return vertices_.size();
}

//////////////////IDBYPOINT////////////////////////////////////////////////
// Find a leaf node where a ra/dec points to
inline uint64
SpatialIndex::idByPoint(const float64 & ra, const float64 & dec) const {
  SpatialVector v(ra,dec);
  return idByPoint(v);
}

//////////////////NAMEBYPOINT//////////////////////////////////////////////
// Find a leaf node where a ra/dec points to, return its name
//

inline char*
SpatialIndex::nameByPoint(const float64 & ra, const float64 & dec, 
			  char* name) const {
  return nameById(idByPoint(ra,dec), name);
}

//////////////////NAMEBYPOINT//////////////////////////////////////////////
// Find a leaf node where v points to, return its name
//

inline char*
SpatialIndex::nameByPoint(SpatialVector & vector, char* name) const {
  return nameById(idByPoint(vector),name);
}
