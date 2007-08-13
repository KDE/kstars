//#     Filename:       SpatialConstraint.cpp
//#
//#     The SpatialConstraint, SpatialSign
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

#include "SpatialConstraint.h"
#include "SpatialException.h"

#include "Htmio.h"
// ===========================================================================
//
// Member functions for class SpatialSign
//
// ===========================================================================

/////////////CONSTRUCTOR//////////////////////////////////
//
SpatialSign::SpatialSign(Sign sign) : sign_(sign) {
}

/////////////COPY CONSTRUCTOR/////////////////////////////
//
SpatialSign::SpatialSign(const SpatialSign & oldSign) : sign_(oldSign.sign_) {
}

/////////////ASSIGNMENT///////////////////////////////////
//
SpatialSign &
SpatialSign::operator =(const SpatialSign & oldSign) {
  if( & oldSign != this)sign_ = oldSign.sign_;
  return *this;
}

// ===========================================================================
//
// Member functions for class SpatialConstraint
//
// ===========================================================================

/////////////CONSTRUCTOR//////////////////////////////////
//
SpatialConstraint::SpatialConstraint(SpatialVector a, float64 d) :
  a_(a), d_(d)
{
  a_.normalize();
  s_ = acos(d_);
  if(d_ <= -gEpsilon) sign_ = nEG;
  if(d_ >=  gEpsilon) sign_ = pOS;
}

/////////////COPY CONSTRUCTOR/////////////////////////////
//
SpatialConstraint::SpatialConstraint(const SpatialConstraint & old) :
  a_(old.a_), d_(old.d_), s_(old.s_) {
  sign_ = old.sign_;
}

/////////////ASSIGNMENT///////////////////////////////////
//
SpatialConstraint &
SpatialConstraint::operator =(const SpatialConstraint & old)
{
  if ( &old != this ) { // beware of self-assignment
    a_ = old.a_;
    d_ = old.d_;
    s_ = old.s_;
    sign_ = old.sign_;
  }
  return *this;
}

/////////////CONTAINS/////////////////////////////////////
// check whether a vector is inside this
//
bool 
SpatialConstraint::contains(const SpatialVector v) {
    if ( acos(v * a_) < s_ ) return true;
    return false;
}

/////////////INVERT///////////////////////////////////////
//
void
SpatialConstraint::invert() {
  d_ = -d_;
  s_ = acos(d_);
  if(sign_ == nEG) sign_ = pOS;
  if(sign_ == pOS) sign_ = nEG;
}

/////////////WRITE////////////////////////////////////////
//
void
SpatialConstraint::write(std::ostream &out) const {
  size_t p = out.precision();
  out.precision(16);
  out << a_ << ' ' << d_ << std::endl;
  out.precision(p);
}

/////////////>>///////////////////////////////////////////
// read from istream
//
std::istream& operator >>( std::istream& in, SpatialConstraint & c) {
  Htmio::read(in, c);
  return(in);
}

/////////////<<///////////////////////////////////////////
// write to ostream
//
std::ostream& operator <<( std::ostream& out, const SpatialConstraint & c) {
  c.write(out);
  return(out);
}
