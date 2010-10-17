//#     Filename:       SpatialConvex.hxx
//#
//#     H definitions for  SpatialConvex
//#
//#
//#     Author:         Peter Z. Kunszt, based on A. Szalay's code
//#     
//#     Date:           October 16, 1998
//#
//#		Copyright (C) 2000  Peter Z. Kunszt, Alex S. Szalay, Aniruddha R. Thakar
//#                     The Johns Hopkins University
//#

extern istream& operator >>( istream&, SpatialConvex &);
extern ostream& operator <<( ostream&, const SpatialConvex &);


inline
SpatialConstraint &
SpatialConvex::operator [](size_t i) {
  return constraints_[i];
}

inline
size_t
SpatialConvex::numConstraints() {
  return constraints_.size();
}
