/*----------------------------------------------------------------------------*/
/**
   @file	fits_std.h
   @author	N. Devillard
   @date	November 2001
   @version	$Revision$
   @brief

   This header file gathers a number of definitions strictly related
   to the FITS format. Include it if you need to get definitions for
   a FITS line size, header metrics, and other FITS-only symbols.
*/
/*----------------------------------------------------------------------------*/

/*
	$Id$
	$Author$
	$Date$
	$Revision$
*/

#ifndef FITS_STD_H
#define FITS_STD_H

/*-----------------------------------------------------------------------------
   								Defines
 -----------------------------------------------------------------------------*/

/* <dox> */
/* FITS header constants */

/** FITS block size */
#define FITS_BLOCK_SIZE     (2880)
/** FITS number of cards per block */
#define FITS_NCARDS 		(36)
/** FITS size of each line in bytes */
#define FITS_LINESZ 		(80)

/** FITS magic number */
#define FITS_MAGIC			"SIMPLE"
/** Size of the FITS magic number */
#define FITS_MAGIC_SZ		6


/* FITS pixel depths */

/** FITS BITPIX=8 */
#define BPP_8_UNSIGNED        (8)
/** FITS BITPIX=16 */
#define BPP_16_SIGNED        (16)
/** FITS BITPIX=32 */
#define BPP_32_SIGNED        (32)
/** FITS BITPIX=-32 */
#define BPP_IEEE_FLOAT      (-32)
/** FITS BITPIX=-64 */
#define BPP_IEEE_DOUBLE     (-64)

/** Default BITPIX for output */
#define BPP_DEFAULT         BPP_IEEE_FLOAT

/** Compute the number of bytes per pixel for a given BITPIX value */
#define BYTESPERPIXEL(x)    (   ((x) == BPP_8_UNSIGNED) ?     1 : \
                                ((x) == BPP_16_SIGNED)  ?     2 : \
                                ((x) == BPP_32_SIGNED)  ?     4 : \
                                ((x) == BPP_IEEE_FLOAT) ?     4 : \
                                ((x) == BPP_IEEE_DOUBLE) ?    8 : 0 ) 
/* </dox> */
#endif
/* vim: set ts=4 et sw=4 tw=75 */
