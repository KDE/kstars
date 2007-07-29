#ifndef _Htmio_h
#define _Htmio_h

#include <RangeConvex.h>
#include <SpatialDomain.h>
#include <SpatialConstraint.h>


//#     Filename:       Htmio.h
//#
//#     Authors:        Gyorgy Fekete,  Will O'Mullane
//#     
//#     Date:           Jan, 2003
//#

//########################################################################
//#
//#  Convex I/O class
//#
/**
*/
class SpatialSign;
class SpatialConstraint;
class RangeConvex;
class SpatialDomain;

class LINKAGE Htmio  {
 public:
  /// Default Constructor
  Htmio();

  static void ignoreCrLf(std::istream &in);
  /// read from stream
  static void read(std::istream&, RangeConvex &rc) ;
  static void read(std::istream&, SpatialDomain &sd) ;
  static void read(std::istream&, SpatialConstraint &sc) ;

  /// read from stream
  static void readRaDec(std::istream&, RangeConvex &rc);
  static void readRaDec(std::istream&, SpatialConstraint &sc);

  /// write to stream
  static void write(std::ostream&, const RangeConvex &rc);

};

#endif
