/*----------------------------------------------------------------------------*/
/**
   @file	pixload.h
   @author	N. Devillard
   @date	Nov 2001
   @version	$Revision$
   @brief	Pixel loader for FITS images.
*/
/*----------------------------------------------------------------------------*/

/*
	$Id$
	$Author$
	$Date$
	$Revision$
*/

#ifndef PIXLOAD_H
#define PIXLOAD_H

/*-----------------------------------------------------------------------------
   								Includes
 -----------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "xmemory.h"
#include "fits_std.h"

/* <dox> */
/*-----------------------------------------------------------------------------
   								Defines
 -----------------------------------------------------------------------------*/

/** Symbol to set returned pixel type to float */
#define PTYPE_FLOAT		0
/** Symbol to set returned pixel type to int */
#define PTYPE_INT		1
/** Symbol to set returned pixel type to double */
#define PTYPE_DOUBLE	2

/*-----------------------------------------------------------------------------
   								New types
 -----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/**
  @brief	Alias for unsigned char

  A 'byte' is just an alias for an unsigned char. It is only defined
  for readability.
 */
/*----------------------------------------------------------------------------*/
typedef unsigned char byte ;

/*----------------------------------------------------------------------------*/
/**
  @brief	qfits loader control object

  This structure serves two purposes: input and output for the qfits
  pixel loading facility. To request pixels from a FITS file, you
  need to allocate (statically or dynamically) such a structure and
  fill up the input fields (filename, xtension number, etc.) to specify
  the pixels you want from the file.

  Before performing the actual load, you must pass the initialized
  structure to qfitsloader_init() which will check whether the operation
  is feasible or not (check its returned value).

  If the operation was deemed feasible, you can proceed to load the pixels,
  passing the same structure to qfits_loadpix() which will fill up the
  output fields of the struct. Notice that a pixel buffer will have been
  allocated (through malloc or mmap) and placed into the structure. You
  need to call free() on this pointer when you are done with it,
  typically in the image or cube destructor.

  The qfitsloader_init() function is also useful to probe a FITS file
  for useful informations, like getting the size of images in the file,
  the pixel depth, or data offset.

  Example of a code that prints out various informations about
  a plane to load, without actually loading it:

  @code
int main(int argc, char * argv[])
{
	qfitsloader	ql ;

	ql.filename = argv[1] ;
	ql.xtnum    = 0 ;
	ql.pnum     = 0 ;

	if (qfitsloader_init(&ql)!=0) {
		printf("cannot read info about %s\n", argv[1]);
		return -1 ;
	}

	printf(	"file         : %s\n"
			"xtnum        : %d\n"
			"pnum         : %d\n"
			"# xtensions  : %d\n"
			"size X       : %d\n"
			"size Y       : %d\n"
			"planes       : %d\n"
			"bitpix       : %d\n"
			"datastart    : %d\n"
            "datasize     : %d\n"
			"bscale       : %g\n"
			"bzero        : %g\n",
			ql.filename,
			ql.xtnum,
			ql.pnum,
			ql.exts,
			ql.lx,
			ql.ly,
			ql.np,
			ql.bitpix,
			ql.seg_start,
            ql.seg_size,
			ql.bscale,
			ql.bzero);
	return 0 ;
}
  @endcode
 */
/*----------------------------------------------------------------------------*/
typedef struct qfitsloader {

	/** Private field to see if structure has been initialized */
	int			_init ;
	
	/** input: Name of the file you want to read pixels from */
	char	*	filename ;
	/** input: xtension number you want to read */
	int			xtnum ;
	/** input: Index of the plane you want, from 0 to np-1 */
	int			pnum ;
	/** input: Pixel type you want (PTYPE_FLOAT, PTYPE_INT or PTYPE_DOUBLE) */
	int			ptype ;
    /** input: Guarantee file copy or allow file mapping */
    int         map ;

	/** output: Total number of extensions found in file */
	int			exts ;
	/** output: Size in X of the requested plane */
	int			lx ;
	/** output: Size in Y of the requested plane */
	int			ly ;
	/** output: Number of planes present in this extension */
	int			np ;
	/** output: BITPIX for this extension */
	int			bitpix ;
	/** output: Start of the data segment (in bytes) for your request */
	int			seg_start ;
    /** output: Size of the data segment (in bytes) for your request */
    int         seg_size ;
	/** output: BSCALE found for this extension */
	double		bscale ;
	/** output: BZERO found for this extension */
	double		bzero ;

	/** output: Pointer to pixel buffer loaded as integer values */
	int		*	ibuf ;
	/** output: Pointer to pixel buffer loaded as float values */
	float	*	fbuf ;
	/** output: Pointer to pixel buffer loaded as double values */
	double	*	dbuf ;

} qfitsloader ;


/*----------------------------------------------------------------------------*/
/**
  @brief	qfits dumper control object

  This structure offers various control parameters to dump a pixel
  buffer to a FITS file. The buffer will be dumped as requested
  to the requested file in append mode. Of course, the requested file
  must be writeable for the operation to succeed.

  The following example demonstrates how to save a linear ramp sized
  100x100 to a FITS file with BITPIX=16. Notice that this code only
  dumps the pixel buffer, no header information is provided in this
  case.

  @code
    int   i, j ;
	int * ibuf ;
	qfitsdumper	qd ;

    // Fill a buffer with 100x100 int pixels
	ibuf = malloc(100 * 100 * sizeof(int));
	for (j=0 ; j<100 ; j++) {
		for (i=0 ; i<100 ; i++) {
			ibuf[i+j*100] = i+j ;
		}
	}

	qd.filename  = "out.fits" ;     // Output file name
	qd.npix      = 100 * 100 ;      // Number of pixels
	qd.ptype     = PTYPE_INT ;      // Input buffer type
	qd.ibuf      = ibuf ;           // Set buffer pointer
	qd.out_ptype = BPP_16_SIGNED ;  // Save with BITPIX=16

    // Dump buffer to file (error checking omitted for clarity)
    qfits_pixdump(&qd);

	free(ibuf);
  @endcode
  
  If the provided output file name is "STDOUT" (all capitals), the
  function will dump the pixels to the stdout steam (usually the console,
  could have been re-directed).
 */
/*----------------------------------------------------------------------------*/
typedef struct qfitsdumper {

	/** Name of the file to dump to, "STDOUT" to dump to stdout */
	char 	*	filename ;
	/** Number of pixels in the buffer to dump */
	int			npix ;
	/** Buffer type: PTYPE_FLOAT, PTYPE_INT or PTYPE_DOUBLE */
	int			ptype ;

	/** Pointer to input integer pixel buffer */
	int		*	ibuf ;
	/** Pointer to input float pixel buffer */
	float	*	fbuf ;
	/** Pointer to input double pixel buffer */
	double	*	dbuf ;

	/** Requested BITPIX in output FITS file */
	int			out_ptype ;
} qfitsdumper ;


/*-----------------------------------------------------------------------------
   							Function prototypes
 -----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/**
  @brief	Initialize a qfitsloader control object.
  @param	ql	qfitsloader object to initialize.
  @return	int 0 if Ok, -1 if error occurred.

  This function expects a qfitsloader object with a number of input
  fields correctly filled in. The minimum fields to set are:

  - filename: Name of the file to examine.
  - xtnum: Extension number to examine (0 for main section).
  - pnum: Plane number in the requested extension.

  You can go ahead with these fields only if you only want to get
  file information for this plane in this extension. If you want
  to later load the plane, you must additionally fill the 'ptype'
  field to a correct value (PTYPE_INT, PTYPE_FLOAT, PTYPE_DOUBLE)
  before calling qfits_loadpix() so that it knows which conversion
  to perform.

  This function is basically a probe sent on a FITS file to ask
  qfits if loading these data would be Ok or not. The actual loading
  is performed by qfits_loadpix() afterwards.
 */
/*----------------------------------------------------------------------------*/
int qfitsloader_init(qfitsloader * ql);

/*----------------------------------------------------------------------------*/
/**
  @brief	Load a pixel buffer for one image.
  @param	ql	Allocated and initialized qfitsloader control object.
  @return	int 0 if Ok, -1 if error occurred.

  This function performs a load of a pixel buffer into memory. It
  expects an allocated and initialized qfitsloader object in input.
  See qfitsloader_init() about initializing the object.

  This function will fill up the ibuf/fbuf/dbuf field, depending
  on the requested pixel type (resp. int, float or double).

  The returned buffer has been allocated using one of the special
  memory operators present in xmemory.c. To deallocate the buffer,
  you must call the version of free() offered by xmemory, not the
  usual system free(). It is enough to include "xmemory.h" in your
  code before you make calls to the pixel loader here.
 */
/*----------------------------------------------------------------------------*/
int qfits_loadpix(qfitsloader * ql);

/*----------------------------------------------------------------------------*/
/**
  @brief    Load a pixel buffer as floats.
  @param    p_source    Pointer to source buffer (as bytes).
  @param    npix        Number of pixels to load.
  @param    bitpix      FITS BITPIX in original file.
  @param    bscale      FITS BSCALE in original file.
  @param    bzero       FITS BZERO in original file.
  @return   1 pointer to a newly allocated buffer of npix floats.

  This function takes in input a pointer to a byte buffer as given
  in the original FITS file (big-endian format). It converts the
  buffer to an array of float (whatever representation is used for
  floats by this platform is used) and returns the newly allocated
  buffer, or NULL if an error occurred.

  The returned buffer must be deallocated using the free() offered
  by xmemory. It is enough to #include "xmemory.h" before calling
  free on the returned pointer.
 */
/*----------------------------------------------------------------------------*/
float  * qfits_pixin_float (byte *, int, int, double, double);

/*----------------------------------------------------------------------------*/
/**
  @brief    Load a pixel buffer as ints.
  @param    p_source    Pointer to source buffer (as bytes).
  @param    npix        Number of pixels to load.
  @param    bitpix      FITS BITPIX in original file.
  @param    bscale      FITS BSCALE in original file.
  @param    bzero       FITS BZERO in original file.
  @return   1 pointer to a newly allocated buffer of npix ints.

  This function takes in input a pointer to a byte buffer as given
  in the original FITS file (big-endian format). It converts the
  buffer to an array of int (whatever representation is used for
  int by this platform is used) and returns the newly allocated
  buffer, or NULL if an error occurred.

  The returned buffer must be deallocated using the free() offered
  by xmemory. It is enough to #include "xmemory.h" before calling
  free on the returned pointer.
 */
/*----------------------------------------------------------------------------*/
int    * qfits_pixin_int   (byte *, int, int, double, double);

/*----------------------------------------------------------------------------*/
/**     
  @brief    Load a pixel buffer as doubles.
  @param    p_source    Pointer to source buffer (as bytes).
  @param    npix        Number of pixels to load.
  @param    bitpix      FITS BITPIX in original file.
  @param    bscale      FITS BSCALE in original file.
  @param    bzero       FITS BZERO in original file.
  @return   1 pointer to a newly allocated buffer of npix doubles.

  This function takes in input a pointer to a byte buffer as given
  in the original FITS file (big-endian format). It converts the
  buffer to an array of double (whatever representation is used for
  int by this platform is used) and returns the newly allocated
  buffer, or NULL if an error occurred.

  The returned buffer must be deallocated using the free() offered
  by xmemory. It is enough to #include "xmemory.h" before calling
  free on the returned pointer.
 */     
/*----------------------------------------------------------------------------*/
double * qfits_pixin_double(byte *, int, int, double, double);

/*----------------------------------------------------------------------------*/
/**
  @brief    Dump a pixel buffer to an output FITS file in append mode.
  @param    qd  qfitsdumper control object.
  @return   int 0 if Ok, -1 otherwise.

  This function takes in input a qfitsdumper control object. This object
  must be allocated beforehand and contain valid references to the data
  to save, and how to save it.

  The minimum fields to fill are:

  - filename: Name of the FITS file to dump to.
  - npix: Number of pixels in the buffer to be dumped.
  - ptype: Type of the passed buffer (PTYPE_FLOAT, PTYPE_INT, PTYPE_DOUBLE)
  - out_ptype: Requested FITS BITPIX for the output.

  One of the following fields must point to the corresponding pixel
  buffer:

  - ibuf for an int pixel buffer (ptype=PTYPE_INT)
  - fbuf for a float pixel buffer (ptype=PTYPE_FLOAT)
  - dbuf for a double pixel buffer (ptype=PTYPE_DOUBLE)

  This is a fairly low-level function, in the sense that it does not
  check that the output file already contains a proper header or even
  that the file it is appending to is indeed a FITS file. It will
  convert the pixel buffer to the requested BITPIX type and append
  the data to the file, without padding with zeros. See qfits_zeropad()
  about padding.

  If the given output file name is "STDOUT" (all caps), the dump
  will be performed to stdout.
 */
/*----------------------------------------------------------------------------*/
int qfits_pixdump(qfitsdumper * qd) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Convert a float pixel buffer to a byte buffer.
  @param    buf     Input float buffer.
  @param    npix    Number of pixels in the input buffer.
  @param    ptype   Requested output BITPIX type.
  @return   1 pointer to a newly allocated byte buffer.
    
  This function converts the given float buffer to a buffer of bytes
  suitable for dumping to a FITS file (i.e. big-endian, in the
  requested pixel type). The returned pointer must be deallocated
  using the free() function offered by xmemory.
 */ 
/*----------------------------------------------------------------------------*/
byte * qfits_pixdump_float(float * buf, int npix, int ptype);

/*----------------------------------------------------------------------------*/
/**
  @brief    Convert an int pixel buffer to a byte buffer.
  @param    buf     Input int buffer.
  @param    npix    Number of pixels in the input buffer.
  @param    ptype   Requested output BITPIX type.
  @return   1 pointer to a newly allocated byte buffer.

  This function converts the given int buffer to a buffer of bytes
  suitable for dumping to a FITS file (i.e. big-endian, in the
  requested pixel type). The returned pointer must be deallocated
  using the free() function offered by xmemory.
 */
/*----------------------------------------------------------------------------*/
byte * qfits_pixdump_int(int * buf, int npix, int ptype);

/*----------------------------------------------------------------------------*/
/**
  @brief    Convert a double pixel buffer to a byte buffer.
  @param    buf     Input double buffer.
  @param    npix    Number of pixels in the input buffer.
  @param    ptype   Requested output BITPIX type.
  @return   1 pointer to a newly allocated byte buffer.

  This function converts the given double buffer to a buffer of bytes
  suitable for dumping to a FITS file (i.e. big-endian, in the
  requested pixel type). The returned pointer must be deallocated
  using the free() function offered by xmemory.
 */
/*----------------------------------------------------------------------------*/
byte * qfits_pixdump_double(double * buf, int npix, int ptype);

/* </dox> */
#endif
/* vim: set ts=4 et sw=4 tw=75 */
