#ifndef _SpatialSign_h_
#define _SpatialSign_h_
//########################################################################
//#
//# Spatial Sign helper class

/**
   The sign class is inherited by Constraint and Convex. Assignment and
   copy operators are used in both scopes.
*/

class LINKAGE SpatialSign {
 public:
  enum Sign {
    nEG,			// All constraints negative or zero
    zERO,			// All constraints zero
    pOS,			// All constraints positive or zero
    mIXED			// At least one pos and one neg
  };

  /// Constructor
  SpatialSign(Sign sign = zERO);

  /// Copy constructor
  SpatialSign(const SpatialSign &);

  /// Assignment
  SpatialSign & operator =(const SpatialSign &);

  // Destructor
  virtual ~SpatialSign(){
  }

 protected:
  /// Sign value
  Sign sign_;
};

#endif
