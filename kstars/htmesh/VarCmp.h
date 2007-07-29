#ifndef _VARCMP_H
#define _VARCMP_H

//#		Filename:		VarCmp.h
//#
//#		Author:			Peter Z Kunszt
//#
//#		Date:			Jan 5 2001
//#
//#		Copyright (C) 2000  Peter Z. Kunszt, Alex S. Szalay, Aniruddha R. Thakar
//#                     The Johns Hopkins University
//#
//#     Modification History:
//#
//#     Oct 18, 2001 : Dennis C. Dinge -- Replaced ValVec with std::vector
//#

#include "SpatialGeneral.h"

/** Functions to compare standard values are provided here.
    These can be used by VarVec<>::sort()
*/

int varCmpInt8( const void *, const void * );
int varCmpInt16( const void *, const void * );
int varCmpInt32( const void *, const void * );
int varCmpInt64( const void *, const void * );
int varCmpUint8( const void *, const void * );
int varCmpUint16( const void *, const void * );
int varCmpUint32( const void *, const void * );
int varCmpUint64( const void *, const void * );
int varCmpFloat32( const void *, const void * );
int varCmpFloat64( const void *, const void * );


#endif
