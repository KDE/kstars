#ifndef _SpatialVector_h
#define _SpatialVector_h

//#     Filename:       SpatialVector.h
//#
//#     Standard 3-d vector class
//#
//#     Author:         Peter Z. Kunszt, based on A. Szalay's code
//#
//#     Date:           October 15, 1998
//#
//#	    SPDX-FileCopyrightText: 2000 Peter Z. Kunszt Alex S. Szalay, Aniruddha R. Thakar
//#                     The Johns Hopkins University
//#
//#     Modification History:
//#
//#     Oct 18, 2001 : Dennis C. Dinge -- Replaced ValVec with std::vector
//#

#include <cmath>
#include <SpatialGeneral.h>

//########################################################################
/**
   @class SpatialVector
   SpatialVector is a 3D vector usually living on the surface of
   the sphere. The corresponding ra, dec can be obtained if the vector
   has unit length. That can be ensured with the normalize() function.

*/

class LINKAGE SpatialVector
{
  public:
    /// constructs (1,0,0), ra=0, dec=0.
    SpatialVector();

    /// Constructor from three coordinates, not necessarily normed to 1
    SpatialVector(float64 x, float64 y, float64 z);

    /// Constructor from ra/dec, this is always normed to 1
    SpatialVector(float64 ra, float64 dec);

    /// Set member function: set values - always normed to 1
    void set(const float64 &x, const float64 &y, const float64 &z);

    /// Set member function: set values - always normed to 1
    void set(const float64 &ra, const float64 &dec);

    /// Get x,y,z
    void get(float64 &x, float64 &y, float64 &z) const;

    /// Get ra,dec - normalizes x,y,z
    void get(float64 &ra, float64 &dec);

    /// return length of vector
    float64 length() const;

    /// return x (only as rvalue)
    float64 x() const { return x_; }

    /// return y
    float64 y() const { return y_; }

    /// return z
    float64 z() const { return z_; }

    /// return ra - this norms the vector to 1 if not already done so
    float64 ra();

    /// return dec - this norms the vector to 1 if not already done so
    float64 dec();

    /// Normalize vector length to 1
    void normalize();

    /// Comparison
    int operator==(const SpatialVector &) const;

    /// dot product
    float64 operator*(const SpatialVector &)const;

    /// cross product
    SpatialVector operator^(const SpatialVector &) const;

    /// addition
    SpatialVector operator+(const SpatialVector &) const;

    /// subtraction
    SpatialVector operator-(const SpatialVector &) const;

    /** @name Scalar products with int and float */
    //@{
    /** @name operator *= */
    SpatialVector &operator*=(float64);
    SpatialVector &operator*=(int);
    friend SpatialVector operator*(float64, const SpatialVector &);
    friend SpatialVector operator*(int, const SpatialVector &);
    friend SpatialVector operator*(const SpatialVector &, float64);
    friend SpatialVector operator*(const SpatialVector &, int);
    //@}

  private:
    float64 x_;
    float64 y_;
    float64 z_;
    float64 ra_;
    float64 dec_;
    bool okRaDec_;

    void updateXYZ();
    void updateRaDec();

    friend class SpatialIndex;
};

// Friend operators
SpatialVector operator*(float64, const SpatialVector &);
SpatialVector operator*(int, const SpatialVector &);
SpatialVector operator*(const SpatialVector &, float64);
SpatialVector operator*(const SpatialVector &, int);

#endif
