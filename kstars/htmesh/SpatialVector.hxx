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
