//#     Filename:       SpatialConstraint.hxx
//#
//#     H implementations for spatialconstraint
//#
//#     Author:         Peter Z. Kunszt, based on A. Szalay's code
//#     
//#     Date:           October 16, 1998
//#
//#		Copyright (C) 2000  Peter Z. Kunszt, Alex S. Szalay, Aniruddha R. Thakar
//#                     The Johns Hopkins University
//#

extern std::istream& operator >>( std::istream&, SpatialConstraint &);
extern std::ostream& operator <<( std::ostream&, const SpatialConstraint &);

// 
inline
SpatialVector &
SpatialConstraint::v() {
  return a_;
}

inline
float64
SpatialConstraint::d() const {
  return d_;
}

inline
void
SpatialConstraint::setVector(SpatialVector &v) {
  a_.set(v.x(),v.y(),v.z());
}

inline
void
SpatialConstraint::setDistance(float64 d) {
  d_ = d;
}

