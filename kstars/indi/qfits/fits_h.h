/*----------------------------------------------------------------------------*/
/**
  @file     fits_h.h
  @author   N. Devillard
  @date     Mar 2000
  @version  $Revision$
  @brief    FITS header handling

  This file contains definition and related methods for the FITS header
  structure. This structure is meant to remain opaque to the user, who
  only accesses it through the dedicated functions offered in this module.
*/
/*----------------------------------------------------------------------------*/

/*
	$Id$
	$Author$
	$Date$
	$Revision$
*/

#ifndef FITS_HEADER_H
#define FITS_HEADER_H

#ifdef __cplusplus
extern "C" {
#endif

/*-----------------------------------------------------------------------------
   								Includes
 -----------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*-----------------------------------------------------------------------------
   								New types
 -----------------------------------------------------------------------------*/

/* <dox> */
/*----------------------------------------------------------------------------*/
/**
  @brief	FITS header object

  This structure represents a FITS header in memory. It is actually no
  more than a thin layer on top of the keytuple object. No field in this
  structure should be directly modifiable by the user, only through
  accessor functions.
 */
/*----------------------------------------------------------------------------*/
typedef struct qfits_header {
	void *	first ;		/* Pointer to list start */
	void *	last ;		/* Pointer to list end */
	int			n ;			/* Number of cards in list */
} qfits_header ;

/*-----------------------------------------------------------------------------
						Function ANSI prototypes
 -----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/**
  @brief    FITS header constructor
  @return   1 newly allocated (empty) FITS header object.

  This is the main constructor for a qfits_header object. It returns
  an allocated linked-list handler with an empty card list.
 */
/*----------------------------------------------------------------------------*/
qfits_header * qfits_header_new(void) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    FITS header default constructor.
  @return   1 newly allocated qfits_header object.

  This is a secondary constructor for a qfits_header object. It returns
  an allocated linked-list handler containing two cards: the first one
  (SIMPLE=T) and the last one (END).
 */
/*----------------------------------------------------------------------------*/
qfits_header * qfits_header_default(void) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Add a new card to a FITS header
  @param    hdr qfits_header object to modify
  @param    key FITS key
  @param    val FITS value
  @param    com FITS comment
  @param    lin FITS original line if exists
  @return   void

  This function adds a new card into a header, at the one-before-last
  position, i.e. the entry just before the END entry if it is there.
  The key must always be a non-NULL string, all other input parameters
  are allowed to get NULL values.
 */
/*----------------------------------------------------------------------------*/
void qfits_header_add(
    qfits_header * hdr,
    char    * key,
    char    * val,
    char    * com,
    char    * lin) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    add a new card to a FITS header
  @param    hdr     qfits_header object to modify
  @param    after   Key to specify insertion place
  @param    key     FITS key
  @param    val     FITS value
  @param    com     FITS comment
  @param    lin     FITS original line if exists
  @return   void

  Adds a new card to a FITS header, after the specified key. Nothing
  happens if the specified key is not found in the header. All fields
  can be NULL, except after and key.
 */
/*----------------------------------------------------------------------------*/
void qfits_header_add_after(
    qfits_header * hdr,
    char    * after,
    char    * key,
    char    * val,
    char    * com,
    char    * lin) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Append a new card to a FITS header.
  @param    hdr qfits_header object to modify
  @param    key FITS key
  @param    val FITS value
  @param    com FITS comment
  @param    lin FITS original line if exists
  @return   void

  Adds a new card in a FITS header as the last one. All fields can be
  NULL except key.
 */
/*----------------------------------------------------------------------------*/
void qfits_header_append(
    qfits_header *   hdr,
    char    * key,
    char    * val,
    char    * com,
    char    * lin) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Delete a card in a FITS header.
  @param    hdr qfits_header to modify
  @param    key specifies which card to remove
  @return   void

  Removes a card from a FITS header. The first found card that matches
  the key is removed.
 */
/*----------------------------------------------------------------------------*/
void qfits_header_del(qfits_header * hdr, char * key) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Modifies a FITS card.
  @param    hdr qfits_header to modify
  @param    key FITS key
  @param    val FITS value
  @param    com FITS comment
  @return   void

  Finds the first card in the header matching 'key', and replaces its
  value and comment fields by the provided values. The initial FITS
  line is set to NULL in the card.
 */
/*----------------------------------------------------------------------------*/
void qfits_header_mod(qfits_header * hdr, char * key, char * val, char * com) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Sort a FITS header
  @param    hdr     Header to sort (modified)
  @return   -1 in error case, 0 otherwise
 */
/*----------------------------------------------------------------------------*/
int qfits_header_sort(qfits_header ** hdr) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Copy a FITS header
  @param    src Header to replicate
  @return   Pointer to newly allocated qfits_header object.

  Makes a strict copy of all information contained in the source
  header. The returned header must be freed using qfits_header_destroy.
 */
/*----------------------------------------------------------------------------*/
qfits_header * qfits_header_copy(qfits_header * src) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Touch all cards in a FITS header
  @param    hdr qfits_header to modify
  @return   void

  Touches all cards in a FITS header, i.e. all original FITS lines are
  freed and set to NULL. Useful when a header needs to be reformatted.
 */
/*----------------------------------------------------------------------------*/
void qfits_header_touchall(qfits_header * hdr) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Dump a FITS header to stdout
  @param    hdr qfits_header to dump
  @return   void

  Dump a FITS header to stdout. Mostly for debugging purposes.
 */
/*----------------------------------------------------------------------------*/
void qfits_header_consoledump(qfits_header * hdr) ; 

/*----------------------------------------------------------------------------*/
/**
  @brief    qfits_header destructor
  @param    hdr qfits_header to deallocate
  @return   void

  Frees all memory associated to a given qfits_header object.
 */
/*----------------------------------------------------------------------------*/
void qfits_header_destroy(qfits_header * hdr) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Return the value associated to a key, as a string
  @param    hdr qfits_header to parse
  @param    key key to find
  @return   pointer to statically allocated string

  Finds the value associated to the given key and return it as a
  string. The returned pointer is statically allocated, so do not
  modify its contents or try to free it.

  Returns NULL if no matching key is found or no value is attached.
 */
/*----------------------------------------------------------------------------*/
char * qfits_header_getstr(qfits_header * hdr, char * key) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Find a matching key in a header.
  @param    hdr qfits_header to parse
  @param    key Key prefix to match
  @return   pointer to statically allocated string.

  This function finds the first keyword in the given header for 
  which the given 'key' is a prefix, and returns the full name
  of the matching key (NOT ITS VALUE). This is useful to locate
  any keyword starting with a given prefix. Careful with HIERARCH
  keywords, the shortFITS notation is not likely to be accepted here.

  Examples:

  @verbatim
  s = qfits_header_findmatch(hdr, "SIMP") returns "SIMPLE"
  s = qfits_header_findmatch(hdr, "HIERARCH ESO DET") returns
  the first detector keyword among the HIERACH keys.
  @endverbatim
 */
/*----------------------------------------------------------------------------*/
char * qfits_header_findmatch(qfits_header * hdr, char * key) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Return the i-th key/val/com/line tuple in a header.
  @param    hdr     Header to consider
  @param    idx     Index of the requested card
  @param    key     Output key
  @param    val     Output value
  @param    com     Output comment
  @param    lin     Output initial line
  @return   int 0 if Ok, -1 if error occurred.

  This function is useful to browse a FITS header object card by card.
  By iterating on the number of cards (available in the 'n' field of
  the qfits_header struct), you can retrieve the FITS lines and their
  components one by one. Indexes run from 0 to n-1. You can pass NULL
  values for key, val, com or lin if you are not interested in a
  given field.

  @code
  int i ;
  char key[FITS_LINESZ+1] ;
  char val[FITS_LINESZ+1] ;
  char com[FITS_LINESZ+1] ;
  char lin[FITS_LINESZ+1] ;

  for (i=0 ; i<hdr->n ; i++) {
    qfits_header_getitem(hdr, i, key, val, com, lin);
    printf("card[%d] key[%s] val[%s] com[%s]\n", i, key, val, com);
  }
  @endcode

  This function has primarily been written to interface a qfits_header
  object to other languages (C++/Python). If you are working within a
  C program, you should use the other header manipulation routines
  available in this module.
 */
/*----------------------------------------------------------------------------*/
int qfits_header_getitem(
        qfits_header    *   hdr,
        int                 idx,
        char            *   key,
        char            *   val,
        char            *   com,
        char            *   lin) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Return the FITS line associated to a key, as a string
  @param    hdr qfits_header to parse
  @param    key key to find
  @return   pointer to statically allocated string

  Finds the FITS line associated to the given key and return it as a
  string. The returned pointer is statically allocated, so do not
  modify its contents or try to free it.

  Returns NULL if no matching key is found or no line is attached.
 */
/*----------------------------------------------------------------------------*/
char * qfits_header_getline(qfits_header * hdr, char * key) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Return the comment associated to a key, as a string
  @param    hdr qfits_header to parse
  @param    key key to find
  @return   pointer to statically allocated string
  @doc

  Finds the comment associated to the given key and return it as a
  string. The returned pointer is statically allocated, so do not
  modify its contents or try to free it.

  Returns NULL if no matching key is found or no comment is attached.
 */
/*----------------------------------------------------------------------------*/
char * qfits_header_getcom(qfits_header * hdr, char * key) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Return the value associated to a key, as an int
  @param    hdr qfits_header to parse
  @param    key key to find
  @param    errval default value to return if nothing is found
  @return   int

  Finds the value associated to the given key and return it as an
  int. Returns errval if no matching key is found or no value is
  attached.
 */
/*----------------------------------------------------------------------------*/
int qfits_header_getint(qfits_header * hdr, char * key, int errval) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Return the value associated to a key, as a double
  @param    hdr qfits_header to parse
  @param    key key to find
  @param    errval default value to return if nothing is found
  @return   double

  Finds the value associated to the given key and return it as a
  double. Returns errval if no matching key is found or no value is
  attached.
 */
/*----------------------------------------------------------------------------*/
double qfits_header_getdouble(qfits_header * hdr, char * key, double errval) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Return the value associated to a key, as a boolean (int).
  @param    hdr qfits_header to parse
  @param    key key to find
  @param    errval default value to return if nothing is found
  @return   int

  Finds the value associated to the given key and return it as a
  boolean. Returns errval if no matching key is found or no value is
  attached. A boolean is here understood as an int taking the value 0
  or 1. errval can be set to any other integer value to reflect that
  nothing was found.

  errval is returned if no matching key is found or no value is
  attached.

  A true value is any character string beginning with a 'y' (yes), a
  't' (true) or the digit '1'. A false value is any character string
  beginning with a 'n' (no), a 'f' (false) or the digit '0'.
 */
/*----------------------------------------------------------------------------*/
int qfits_header_getboolean(qfits_header * hdr, char * key, int errval) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Write out a key tuple to a string on 80 chars.
  @param    line    Allocated output character buffer.
  @param    key     Key to write.
  @param    val     Value to write.
  @param    com     Comment to write.
  @return   void

  Write out a key, value and comment into an allocated character buffer.
  The buffer must be at least 80 chars to receive the information.
  Formatting is done according to FITS standard.
 */
/*----------------------------------------------------------------------------*/
void keytuple2str(char * line, char * key, char * val, char * com) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Dump a FITS header to an opened file.
  @param    hdr     FITS header to dump
  @param    out     Opened file pointer
  @return   int 0 if Ok, -1 otherwise

  Dumps a FITS header to an opened file pointer.
 */
/*----------------------------------------------------------------------------*/
int qfits_header_dump(qfits_header * hdr, FILE * out) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Dump a fits header into a memory block.
  @param    fh      FITS header to dump
  @param    hsize   Size of the returned header, in bytes (output).
  @return   1 newly allocated memory block containing the FITS header.

  This function dumps a FITS header structure into a newly allocated
  memory block. The block is composed of characters, just as they would
  appear in a FITS file. This function is useful to make a FITS header
  in memory.

  The returned block size is indicated in the passed output variable
  'hsize'. The returned block must be deallocated using free().
 */
/*----------------------------------------------------------------------------*/
char * qfits_header_to_memblock(qfits_header * fh, int * hsize) ;
/* </dox> */

#ifdef __cplusplus
}
#endif

#endif
/* vim: set ts=4 et sw=4 tw=75 */
