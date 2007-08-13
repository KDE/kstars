//#     Filename:       SpatialVector.cpp
//#
//#     The SpatialVector class is defined here.
//#
//#     Author:         Peter Z. Kunszt based on A. Szalay's code
//#
//#     Date:           October 15, 1998
//#
//#		Copyright (C) 2000  Peter Z. Kunszt, Alex S. Szalay, Aniruddha R. Thakar
//#                     The Johns Hopkins University
//#     Modification History:
//#
//#     Oct 18, 2001 : Dennis C. Dinge -- Replaced ValVec with std::vector
//#

#include "SpatialVector.h"
#include "SpatialException.h"

//==============================================================
//
// This 3D vector lives on the surface of the sphere.
// Its length is always 1.
//
//==============================================================

/////////////CONSTRUCTOR//////////////////////////////////
//
SpatialVector::SpatialVector() : 
  x_(1), y_(0), z_(0), ra_(0), dec_(0), okRaDec_(true) {
}


SpatialVector::SpatialVector(float64 x, float64 y, float64 z) :
  x_(x), y_(y), z_(z), okRaDec_(false) {
}

/////////////CONSTRUCTOR//////////////////////////////////
//
SpatialVector::SpatialVector(float64 ra, float64 dec) :
    ra_(ra), dec_(dec), okRaDec_(true) {
  updateXYZ();
  updateRaDec();
}

/////////////COPY CONSTRUCTOR/////////////////////////////
//
SpatialVector::SpatialVector(const SpatialVector & vv) :
  x_(vv.x_), y_(vv.y_), z_(vv.z_), ra_(vv.ra_), dec_(vv.dec_), 
  okRaDec_(vv.okRaDec_) {
}

/////////////ASSIGNMENT///////////////////////////////////
//
SpatialVector&
SpatialVector::operator =(const SpatialVector & vv)
{
  x_ = vv.x_;
  y_ = vv.y_;
  z_ = vv.z_;
  ra_ = vv.ra_;
  dec_ = vv.dec_;
  okRaDec_ = vv.okRaDec_;
  return *this;
}

/////////////SET//////////////////////////////////////////
//
void
SpatialVector::set(const float64 &x, const float64 &y, const float64 &z )
{
  x_ = x;
  y_ = y;
  z_ = z;
  normalize();
  updateRaDec();
}
/////////////SET//////////////////////////////////////////
//
void
SpatialVector::set(const float64 &ra, const float64 &dec)
{
  ra_ = ra;  
  dec_ = dec;
  updateXYZ();
}

/////////////GET//////////////////////////////////////////
//
void
SpatialVector::get(float64 &x,float64 &y,float64 &z ) const
{
  x = x_;
  y = y_;
  z = z_;
}

/////////////GET//////////////////////////////////////////
//
void
SpatialVector::get(float64 &ra,float64 &dec )
{
  if(!okRaDec_) {
    normalize();
    updateRaDec();
  }
  ra = ra_;
  dec = dec_;
}

float64 SpatialVector::ra() {
  if(!okRaDec_) {
    normalize();
    updateRaDec();
  }
  return ra_;
}

float64 SpatialVector::dec() {
  if(!okRaDec_) {
    normalize();
    updateRaDec();
  }
  return dec_;
}


/////////////NORMALIZE////////////////////////////////////
//
void
SpatialVector::normalize()
{
float64 sum;
   sum = x_*x_ + y_*y_ + z_*z_;
   sum = sqrt(sum);
   x_ /= sum;
   y_ /= sum;
   z_ /= sum;
}

/////////////LENGTH///////////////////////////////////////
//
float64
SpatialVector::length() const
{
float64 sum;
   sum = x_*x_ + y_*y_ + z_*z_;
   return sum > gEpsilon ? sqrt(sum) : 0.0;
}

/////////////UPDATERADEC//////////////////////////////////
//
void
SpatialVector::updateRaDec() {
  dec_ = asin(z_)/gPr; // easy.
  float64 cd = cos(dec_*gPr);
  if(cd>gEpsilon || cd<-gEpsilon)
    if(y_>gEpsilon || y_<-gEpsilon)
      if (y_ < 0.0)
	ra_ = 360 - acos(x_/cd)/gPr;
      else
	ra_ = acos(x_/cd)/gPr;
    else
      ra_ = (x_ < 0.0 ? 180.0 : 0.0);
  else 
    ra_=0.0;
  okRaDec_ = true;
}

/////////////UPDATEXYZ////////////////////////////////////
//
void
SpatialVector::updateXYZ() {
    float64 cd = cos(dec_*gPr);
    x_ = cos(ra_*gPr) * cd;
    y_ = sin(ra_*gPr) * cd;
    z_ = sin(dec_*gPr);
}
/////////////OPERATOR *=//////////////////////////////////
//
SpatialVector&
SpatialVector::operator *=(float64 a)
{
  x_ = a*x_;  
  y_ = a*y_;  
  z_ = a*z_;
  okRaDec_ = false;
  return *this;
}

/////////////OPERATOR *=//////////////////////////////////
//
SpatialVector&
SpatialVector::operator *=(int a)
{
  x_ = a*x_;  
  y_ = a*y_;  
  z_ = a*z_;
  okRaDec_ = false;
  return *this;
}

/////////////OPERATOR *///////////////////////////////////
// Multiply with a number
//
SpatialVector
operator *(float64 a, const SpatialVector& v)
{
  return SpatialVector(a*v.x_, a*v.y_, a*v.z_);
}

/////////////OPERATOR *///////////////////////////////////
// Multiply with a number
//
SpatialVector
operator *(const SpatialVector& v, float64 a)
{
  return SpatialVector(a*v.x_, a*v.y_, a*v.z_);
}

/////////////OPERATOR *///////////////////////////////////
// Multiply with a number
//
SpatialVector
operator *(int a, const SpatialVector& v) 
{
  return SpatialVector(a*v.x_, a*v.y_, a*v.z_);
}

/////////////OPERATOR *///////////////////////////////////
// Multiply with a number
//
SpatialVector
operator *(const SpatialVector& v, int a)
{
  return SpatialVector(a*v.x_, a*v.y_, a*v.z_);
}


/////////////OPERATOR *///////////////////////////////////
// dot product
//
float64
SpatialVector::operator *(const SpatialVector & v) const
{
   return (x_*v.x_)+(y_*v.y_)+(z_*v.z_);
}

/////////////OPERATOR +///////////////////////////////////
//
SpatialVector
SpatialVector::operator +(const SpatialVector & v) const
{
  return SpatialVector(x_+v.x_, y_+v.y_, z_+v.z_);
}

/////////////OPERATOR -///////////////////////////////////
//
SpatialVector
SpatialVector::operator -(const SpatialVector & v) const
{
  return SpatialVector(x_-v.x_, y_-v.y_, z_-v.z_);
}

/////////////OPERATOR ^///////////////////////////////////
// cross product
//
SpatialVector
SpatialVector::operator ^(const SpatialVector &v) const
{
  return SpatialVector(y_ * v.z_ - v.y_ * z_,
		 z_ * v.x_ - v.z_ * x_,
		 x_ * v.y_ - v.x_ * y_);
}

/////////////OPERATOR ==//////////////////////////////////
//
int
SpatialVector::operator ==(const SpatialVector & v) const
{
  return ( (x_ == v.x_ && y_ == v.y_ && z_ == v.z_) ? 1 : 0 );
}

/////////////SHOW/////////////////////////////////////////
// print to stdout
//
void 
SpatialVector::show() const
{
  printf(" %11.8f %11.8f %11.8f \n",x_,y_,z_);
}

/////////////READ/////////////////////////////////////////
// print to stdout
//
void 
SpatialVector::read(std::istream &in)
{
  in.setf(std::ios::skipws);
  in >> x_ >> y_ >> z_;
  if(!in.good())
    throw SpatialFailure("SpatialVector:read: Could not read vector");
}

/////////////WRITE////////////////////////////////////////
// print to stdout
//
void 
SpatialVector::write(std::ostream &out) const
{
  out << x_ << ' ' << y_ << ' ' << z_ ;
}

