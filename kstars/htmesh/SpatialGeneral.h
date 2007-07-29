#if !defined (_SpatialGeneral_h)
#define _SpatialGeneral_h
//#		Filename:	SpatialGeneral.h
//#
//#		This file contains all SDSS Science Archive global information,
//#		including Global Type Definitions, Global Macros, and Global Inlines
//#
//#		Author:		Peter Z. Kunszt
//#	
//#		Date:		October 19, 1999
//#
//#		Copyright (C) 2000  Peter Z. Kunszt, Alex S. Szalay, Aniruddha R. Thakar
//#                     The Johns Hopkins University
//#
//#     Modification History:
//#
//#     Oct 18, 2001 : Dennis C. Dinge -- Replaced ValVec with std::vector:
//#

// Type Definitions

//  define portable int and float types where not already defined
//  The SDSS Science Archive and Objectivity define the very same thing.
//	Objectivity defines these in ooMachine.h (version 4) and
//	in ooConfig.h (version 5).
//

// The following list is for different machines:
//
// ------------------------------------------------------------------------
// MACHINE |  CHAR | SHORT |   INT |  LONG | FLOAT | DOUBLE | LONG DOUBLE |
// ------------------------------------------------------------------------
//  SUN    |    8  |   16  |   32  |   32  |   32  |   64   |   128       |
// ------------------------------------------------------------------------
//  ALPHA  |    8  |   16  |   32  |   64  |   32  |   64   |    64       |
// ------------------------------------------------------------------------
//  SGI-n32|    8  |   16  |   32  |   32  |   32  |   64   |   128       |
// ------------------------------------------------------------------------
//  SGI    |    8  |   16  |   32  |   32  |   32  |   64   |    64       |
// ------------------------------------------------------------------------
//  NT     |    8  |   16  |   32  |   32  |   32  |   64   |    64       |
// ------------------------------------------------------------------------
//  LINUX  |    8  |   16  |   32  |   32  |   32  |   64   |    96       |
// ------------------------------------------------------------------------
//

#if !defined(OO_MACHINE_H) && !defined(OO_CONFIG_H) && !defined(SXGENERAL_H)
typedef char				int8;
typedef unsigned char		uint8;
typedef short int			int16;
typedef unsigned short int	uint16;
typedef int					int32;
typedef unsigned int		uint32;
typedef float				float32;
typedef double				float64;
#ifdef _WIN32
typedef __int64				int64;
typedef unsigned __int64	uint64;
#else
typedef long long			int64;
typedef unsigned long long	uint64;
#endif
#endif

#if defined(__sun) || defined(__sgi)
#   define SPATIALBIGENDIAN
#endif

#define IDSIZE             64
#define HTMNAMEMAX         32
#define HTMMAXDEPTH        25
#define MAX_SQL_DEPTH      20
#define HTMMAXKEEP		   12
#define HTMMAXBIT		   14

// emulate the standard bool type where not supported by compiler

#  if !defined(SXGENERAL_H) && !defined(__sgi) && !defined(__linux) && !defined(_WIN32)
#    ifdef __unix
/*
 * The following ifndef must ALWAYS be present since C++ may use
 * _BOOL_EXISTS to prevent the header from trying to redefine a
 * C++ reserved word.
 */
#      ifndef _BOOL_EXISTS
#         define _BOOL_EXISTS
#         ifndef bool

typedef unsigned char           bool;
const bool                      false = 0;
const bool                      true = 1;
#define bool(x) ((x) ? true : false)

#         endif
#      endif  /* _BOOL_EXISTS */
#    endif  /* __unix */
#  endif  /* __sgi && __linux */

// Global Math Constants

const float64 gPi = 3.1415926535897932385E0 ;
const float64 gPr = gPi/180.0; 
const float64 gEpsilon = 1.0E-15;
// CVSversion = "$Name:  $";

//
// To Simplify Porting, define our platforms:
// Digital UNIX -> SpatialDigitalUnix
// SGI          -> SpatialSGI
// WINNT        -> SpatialWinNT
//
// These are set here, and then included by everything else.

// Flag SpatialStandardTemplate: Defined to indicating proper method
// for explicit template instantiation.  If a compiler supports the
// standard explicit template instantiation, define.
//

#if defined(__macosx)
#   define LINKAGE
#endif

#if defined(__unix)

#   if defined(__osf__)
#      define SpatialDigitalUnix 1
#      define SpatialStandardTemplate 1
#   elif defined(__sgi)
#      define SpatialSGI 1
#      define SpatialPragmaTemplateSGI 1
#   elif defined(__sun)
#      define SpatialSUN 1
#      define SpatialStandardTemplate 1
#   elif defined(__linux)
#      define SpatialLinux 1
#      define SpatialStandardTemplate 1
#   endif
#   define LINKAGE 

#elif defined(_WIN32)
#   define SpatialWinNT 1
#   define SpatialWINTemplate 1
// This warning is about template instances being exported in the dll...
#   pragma warning(disable: 4251)

// Define LINKAGE for NT VC++6 complier
 
#   ifdef _EXPORTING
#	define LINKAGE	__declspec(dllexport)
#   endif
#   ifdef _IMPORTING
#	define	LINKAGE	__declspec(dllimport)
#   endif

#endif  /* _WIN32 */

#ifndef SpatialWinNT
#   define IDHIGHBIT  0x8000000000000000LL
#   define IDHIGHBIT2 0x4000000000000000LL
#   ifdef SpatialDigitalUnix
#      define PRINTID(x) printf("%lu",(x))
#      define PRINTID_HEX(x) printf("%lX",(x))
#   else
#      define PRINTID(x) printf("%llu",(x))
#      define PRINTID_HEX(x) printf("%llX",(x))
#   endif
#else
#   define PRINTID(x) printf("%I64u",(x))
#   define PRINTID_HEX(x) printf("%I64X",(x))
#endif

// Swap functions for little-endian.

#if !defined(SPATIALBIGENDIAN) && !defined(SXGENERAL_H)
#   define SWAP(a,b)     { a^=b; b^=a; a^=b; }
//
// swapEndian was here
//
#endif

#if defined(_WIN32) && defined(_INC_IOSTREAM)
LINKAGE ostream& operator<<(ostream& os, const int64 &i64);
LINKAGE ostream& operator<<(ostream& os, const uint64 &ui64);
#endif

#endif /* SPATIALGENERAL_H */
