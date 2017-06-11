#ifndef _SpatialSign_h_
#define _SpatialSign_h_
//########################################################################
//#
//# Spatial Sign helper class

/** */
enum Sign
{
    nEG,  /// All constraints negative or zero
    zERO, /// All constraints zero
    pOS,  /// All constraints positive or zero
    mIXED /// At least one pos and one neg
};

#endif
