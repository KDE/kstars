/*----------------------------------------------------------------------------*/
/**
   @file    simple.h
   @author  N. Devillard
   @date    Jan 1999
   @version $Revision$
   @brief   Simple FITS access routines.

   This module offers a number of very basic low-level FITS access
   routines.
*/
/*----------------------------------------------------------------------------*/

/*
	$Id$
	$Author$
	$Date$
	$Revision$
*/

#ifndef SIMPLE_H
#define SIMPLE_H

#ifdef __cplusplus
extern "C" {
#endif

/*-----------------------------------------------------------------------------
   								Includes
 -----------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* <dox> */
/*-----------------------------------------------------------------------------
   								Defines
 -----------------------------------------------------------------------------*/

/** Unknown type for FITS value */
#define QFITS_UNKNOWN		0
/** Boolean type for FITS value */
#define QFITS_BOOLEAN		1
/** Int type for FITS value */
#define	QFITS_INT			2
/** Float type for FITS value */
#define QFITS_FLOAT			3
/** Complex type for FITS value */
#define QFITS_COMPLEX		4
/** String type for FITS value */
#define QFITS_STRING			5

/*-----------------------------------------------------------------------------
  							Function codes
 -----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/**
  @brief    Retrieve the value of a key in a FITS header
  @param    filename    Name of the FITS file to browse
  @param    keyword     Name of the keyword to find
  @return   pointer to statically allocated character string

  Provide the name of a FITS file and a keyword to look for. The input
  file is memory-mapped and the first keyword matching the requested one is
  located. The value corresponding to this keyword is copied to a
  statically allocated area, so do not modify it or free it.

  The input keyword is first converted to upper case and expanded to
  the HIERARCH scheme if given in the shortFITS notation.

  This function is pretty fast due to the mmapping. Due to buffering
  on most Unixes, it is possible to call many times this function in a
  row on the same file and do not suffer too much from performance
  problems. If the file contents are already in the cache, the file
  will not be re-opened every time.

  It is possible, though, to modify this function to perform several
  searches in a row. See the source code.

  Returns NULL in case the requested keyword cannot be found.
 */
/*----------------------------------------------------------------------------*/
char * qfits_query_hdr(char * filename, char * keyword) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Retrieve the value of a keyin a FITS extension header.
  @param    filename    name of the FITS file to browse.
  @param    keyword     name of the FITS key to look for.
  @param    xtnum       xtension number
  @return   pointer to statically allocated character string

  Same as qfits_query_hdr but for extensions. xtnum starts from 1 to
  the number of extensions. If xtnum is zero, this function is 
  strictly identical to qfits_query_hdr().
 */
/*----------------------------------------------------------------------------*/
char * qfits_query_ext(char * filename, char * keyword, int xtnum) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Counts the number of extensions in a FITS file
  @param    filename    Name of the FITS file to browse.
  @return   int

  Counts how many extensions are in the file. Returns 0 if no
  extension is found, and -1 if an error occurred.
 */
/*----------------------------------------------------------------------------*/
int qfits_query_n_ext(char * filename) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Counts the number of planes in a FITS extension.
  @param    filename    Name of the FITS file to browse.
  @param    exnum       Extensin number
  @return   int
  Counts how many planes are in the extension. Returns 0 if no plane is found,
  and -1 if an error occurred.
 */
/*----------------------------------------------------------------------------*/
int qfits_query_nplanes(char * filename, int extnum) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Clean out a FITS string value.
  @param    s pointer to allocated FITS value string.
  @return   pointer to statically allocated character string

  From a string FITS value like 'marvin o''hara', remove head and tail
  quotes, replace double '' with simple ', trim blanks on each side,
  and return the result in a statically allocated area.

  Examples:

  - ['o''hara'] becomes [o'hara]
  - ['  H    '] becomes [H]
  - ['1.0    '] becomes [1.0]

 */
/*----------------------------------------------------------------------------*/
char * qfits_pretty_string(char * s) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Identify if a FITS value is boolean
  @param    s FITS value as a string
  @return   int 0 or 1

  Identifies if a FITS value is boolean.
 */
/*----------------------------------------------------------------------------*/
int qfits_is_boolean(char * s) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Identify if a FITS value is an int.
  @param    s FITS value as a string
  @return   int 0 or 1

  Identifies if a FITS value is an integer.
 */
/*----------------------------------------------------------------------------*/
int qfits_is_int(char * s) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Identify if a FITS value is float.
  @param    s FITS value as a string
  @return   int 0 or 1

  Identifies if a FITS value is float.
 */
/*----------------------------------------------------------------------------*/
int qfits_is_float(char * s) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Identify if a FITS value is complex.
  @param    s FITS value as a string
  @return   int 0 or 1

  Identifies if a FITS value is complex.
 */
/*----------------------------------------------------------------------------*/
int qfits_is_complex(char * s) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Identify if a FITS value is string.
  @param    s FITS value as a string
  @return   int 0 or 1

  Identifies if a FITS value is a string.
 */
/*----------------------------------------------------------------------------*/
int qfits_is_string(char * s) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Identify the type of a FITS value given as a string.
  @param    s FITS value as a string
  @return   integer naming the FITS type

  Returns the following value:

  - QFITS_UNKNOWN (0) for an unknown type.
  - QFITS_BOOLEAN (1) for a boolean type.
  - QFITS_INT (2) for an integer type.
  - QFITS_FLOAT (3) for a floating-point type.
  - QFITS_COMPLEX (4) for a complex number.
  - QFITS_STRING (5) for a FITS string.
 */
/*----------------------------------------------------------------------------*/
int qfits_get_type(char * s) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Retrieve offset to start and size of a header in a FITS file.
  @param    filename    Name of the file to examine
  @param    xtnum       Extension number (0 for main)
  @param    seg_start   Segment start in bytes (output)
  @param    seg_size    Segment size in bytes (output)
  @return   int 0 if Ok, -1 otherwise.

  This function retrieves the two most important informations about
  a header in a FITS file: the offset to its beginning, and the size
  of the header in bytes. Both values are returned in the passed
  pointers to ints. It is Ok to pass NULL for any pointer if you do
  not want to retrieve the associated value.

  You must provide an extension number for the header, 0 meaning the
  main header in the file.
 */
/*----------------------------------------------------------------------------*/
int qfits_get_hdrinfo(
        char * filename,
        int    xtnum,
        int  * seg_start,
        int  * seg_size) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Retrieve offset to start and size of a data section in a file.
  @param    filename    Name of the file to examine.
  @param    xtnum       Extension number (0 for main).
  @param    seg_start   Segment start in bytes (output).
  @param    seg_size    Segment size in bytes (output).
  @return   int 0 if Ok, -1 otherwise.

  This function retrieves the two most important informations about
  a data section in a FITS file: the offset to its beginning, and the size
  of the section in bytes. Both values are returned in the passed
  pointers to ints. It is Ok to pass NULL for any pointer if you do
  not want to retrieve the associated value.

  You must provide an extension number for the header, 0 meaning the
  main header in the file.
 */
/*----------------------------------------------------------------------------*/
int qfits_get_datinfo(
        char * filename,
        int    xtnum,
        int  * seg_start,
        int  * seg_size) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Query a card in a FITS (main) header by a given key
  @param    filename    Name of the FITS file to check.
  @param    keyword     Where to read a card in the header.
  @return   Allocated string containing the card or NULL
 */
/*----------------------------------------------------------------------------*/
char * qfits_query_card(
        char    *   filename,
        char    *   keyword) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Replace a card in a FITS (main) header by a given card
  @param    filename    Name of the FITS file to modify.
  @param    keyword     Where to substitute a card in the header.
  @param    substitute  What to replace the line with.
  @return   int 0 if Ok, -1 otherwise

  Replaces a whole card (80 chars) in a FITS header by a given FITS
  line (80 chars). The replacing line is assumed correctly formatted
  and containing at least 80 characters. The file is modified: it must
  be accessible in read/write mode.

  The input keyword is first converted to upper case and expanded to
  the HIERARCH scheme if given in the shortFITS notation. 

  Returns 0 if everything worked Ok, -1 otherwise.
 */
/*----------------------------------------------------------------------------*/
int qfits_replace_card(
        char    *   filename,
        char    *   keyword,
        char    *   substitute) ;

/* </dox> */
#ifdef __cplusplus
}
#endif

#endif
/* vim: set ts=4 et sw=4 tw=75 */
