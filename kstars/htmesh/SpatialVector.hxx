//#     Filename:       SpatialVector.hxx
//#
//#     Standard 3-d vector class: .h implementations
//#
//#     Author:         Peter Z. Kunszt
//#     
//#     Date:           October 15, 1998
//#
//#		Copyright (C) 2000  Peter Z. Kunszt, Alex S. Szalay, Aniruddha R. Thakar
//#                     The Johns Hopkins University
//#

// Friend operators
SpatialVector operator *(float64, const SpatialVector&);
SpatialVector operator *(int, const SpatialVector&);
SpatialVector operator *(const SpatialVector&, float64);
SpatialVector operator *(const SpatialVector&, int);

// inline functions

inline
float64 SpatialVector::x() const {
  return x_;
}

inline
float64 SpatialVector::y() const {
  return y_;
}

inline
float64 SpatialVector::z() const {
  return z_;
}

/////////////>>///////////////////////////////////////////
// read from istream
//
inline
std::istream& operator >>( std::istream& in, SpatialVector & v) {
  v.read(in);
  return(in);
}

/////////////<<///////////////////////////////////////////
// write to ostream
//
inline
std::ostream& operator <<( std::ostream& out, const SpatialVector & v) {
  v.write(out);
  return(out);
}
