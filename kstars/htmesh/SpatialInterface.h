#ifndef _SpatialInterface_h
#define _SpatialInterface_h

//#     Filename:       SpatialInterface.h
//#
//#     Interfaceion interface
//#
//#     Date:           August 31, 2000
//#
//#	Authors: Peter Z. Kunszt, Alex S. Szalay, Aniruddha R. Thakar
//#                     The Johns Hopkins University
//#
//#     Modification History:
//#
//#     Oct 18, 2001 : Dennis C. Dinge -- Replaced ValVec with std::vector
//#	Jul 29, 2002 : Gyorgy (George) Fekete -- added lookupPoint
//#

#include "SpatialIndex.h"
#include "SpatialDomain.h"
#include "VarStr.h"

class htmRange {
public:
  uint64 lo;
  uint64 hi;
};

typedef std::vector<htmRange> ValueVector;
typedef std::vector<float64> ValueVectorF64;

struct htmPolyCorner {
  SpatialVector c_;
  bool inside_;
  bool replace_;
}; 

/**
   htmInterface class.
   The SpatialInterface class contains all methods to interface the
   HTM index with external applications.

*/

class  LINKAGE htmInterface {

public:
	void fillValueVec(HtmRange &hr, ValueVector &vec);

  /** Constructor. The depth is optional, defaulting to level 5. It
      can be changed with the changeDepth() memberfunction or it can
      be specified with one of the string command interfaces. The
      saveDepth parameter can be specified to keep the given amount of
      levels in memory. This can also be altered by changeDepth. */

	htmInterface(size_t depth = 5, size_t saveDepth = 5); // [ed:gyuri:saveDepth was 2]

  /** Destructor. */
  ~htmInterface();

  /** Access the index associated with the interface */
  const SpatialIndex & index() const;

 /** Get a vector xyz from a node ID.
     Given a HTM ID, return a vector to the centroid of the triangle (GYF)
  */
  void pointById(SpatialVector &vec, uint64 htmid) const;

  /** Look at the given htmid, and adjust the spatialindex object's depth (GYF)
    */
  size_t adjustDepthToID(uint64 htmid) ;
 /** Lookup a node ID from ra,dec.
      Given a certain RA,DEC and index depth return its HTM ID.
  */
  uint64 lookupID(float64 ra, float64 dec) const;

  /** Lookup a node ID from x,y,z.
      Given a certain cartesian vector x,y,z and index depth return its HTM ID.
  */
  uint64 lookupID(float64 x, float64 y, float64 z) const;

  /** Lookup the ID from the Name string.
  */
  uint64 lookupID(char *) const;

  /** Lookup a node ID from a string command.
      The string in the input may have one of the following forms:
      <ul>
      <li> "J2000 depth ra dec"
      <li> "CARTESIAN depth x y z"
      <li> "NAME name"
      </ul>
      The string will be evaluated depending on how many items it has.
      SpatialInterfaceError is thrown if the string is unexpected.
  */
  uint64 lookupIDCmd(char *);

  /** Lookup a node name from ra,dec
      Given a certain RA,DEC and index depth return its HTM NodeName.
  */
  const char * lookupName(float64 ra, float64 dec) ;

  /** Lookup a node name from x,y,z.
      Given a certain cartesian vector x,y,z and index depth return its 
      HTM NodeName.
  */
  const char * lookupName(float64 x, float64 y, float64 z) ;

  /** Lookup a node name from a node ID.
  */
  const char * lookupName(uint64 ID) ;

  /** Lookup a node name using a string command.
      The string in the input may have one of the following forms:
      <ul>
      <li> "J2000 depth ra dec"
      <li> "CARTESIAN depth x y z"
      <li> "ID id"
      </ul>
      The string will be evaluated depending on how many items it has.
      SpatialInterfaceError is thrown if the string is unexpected.
  */
  const char * lookupNameCmd(char *);

  /** Request all triangles in a circular region.
      Given are the center coordinate and radius in arcminutes.
  */
  const ValueVector & circleRegion( float64 ra,
					 float64 dec,
					 float64 rad ) ;

  /** Request all triangles in a circular region.
      Given are the center coordinate and radius in arcminutes.
  */
  const ValueVector & circleRegion( float64 x,
					 float64 y,
					 float64 z,
					 float64 rad ) ;

  /** Request all triangles in a circular region.
      Given are the center coordinate and radius in arcminutes.
      Same as previous two functions but from a string.
  */
  const ValueVector & circleRegionCmd( char *str );

  /** Request all triangles in the convex hull of a given set of 
      points.
  */
  const ValueVector & convexHull( ValueVectorF64 ra,
				       ValueVectorF64 dec ) ;

  /** Request all triangles in the convex hull of a given set of 
      points.
  */
  const ValueVector & convexHull( ValueVectorF64 x,
				       ValueVectorF64 y,
				       ValueVectorF64 z ) ;

  /** Request all triangles in the convex hull of a given set of 
      points.
      The points are given in the string in the following form:
      <pre>
      " J2000 depth ra dec ra dec ra dec "  
      </pre>
      or
      <pre>
      " CARTESIAN depth x y z x y z x y z "
      </pre>
      There may be as many points ra, dec or x,y,z as you want.
  */
  const ValueVector & convexHullCmd( char *str );


  /** Give the ranges for an intersection with a proper domain. */
  const ValueVector & domain( SpatialDomain & );

  /** String interface for domain intersection.
      The domain should be given in the following form:
      <pre>
      DOMAIN depth
      nConvexes
      nConstraints in convex 1
      x y z d
      x y z d
      .
      .
      x y z d
      nConstraints in convex 2
      x y z d
      x y z d
      .
      .
      x y z d
      .
      .
      .
      nConstrinats in convex n
      x y z d
      x y z d
      .
      .
      x y z d
      <pre>

      <p>
      The numbers need to be separated by whitespace (newlines are allowed).
      Throws SpatialInterfaceError on syntax errors.
  */
  const ValueVector & domainCmd( char *str );

  /** Change the current index depth */
  void changeDepth(size_t depth, size_t saveDepth = 5);

  /** Check whether a varstring is an integer */
  static bool isInteger(const StdStr &);

  /** Check whether a varstring is a float */
  static bool isFloat(const StdStr &);

  /** Check whether a range contains a certain id */
  static bool inRange( const ValueVector &, uint64 );

  /** Print the ranges to cout */
  static void printRange( const ValueVector & );
private:

  enum cmdCode {
    J2000,
    CARTESIAN,
    NAME,
    ID,
    HTMDOMAIN
  };

  char name_[HTMNAMEMAX];

  SpatialIndex *index_;
  ValueVector range_;
  ValueVectorUint64 idList_;
  typedef std::vector<htmPolyCorner> ValueVectorPolyCor;
  ValueVectorPolyCor polyCorners_;
  StdStr cmd_;
  VarStrToken *t_;

  // parse command code
  cmdCode getCode();

  // parse depth
  void getDepth();

  // parse the string, returning the depth
  bool parseVec( cmdCode, float64 *v );

  int32   getInteger();   // get an int off the command string
  uint64  getInt64();     // get an int off the command string
  float64 getFloat();     // get a float off the command string

  // add a polygon corner to the list, sort it counterclockwise
  // and ignore if inside the convex hull
  void setPolyCorner(SpatialVector &v);

  // this routine does the work for all convexHull calls
  const ValueVector & doHull();

};

#include "SpatialInterface.hxx"
#endif

