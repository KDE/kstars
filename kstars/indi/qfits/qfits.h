#ifndef _QFITS_H_
#define _QFITS_H_


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

extern char * qfits_version(void);


/*----------------------------------------------------------------------------*/
/**
  @brief    Purge the qfits cache.
  @return   void

  This function is useful for programs running for a long period,
  to clean up the cache. Ideally in a daemon, it should be called
  by a timer at regular intervals. Notice that since the cache is
  fairly small, you should not need to care too much about this.
 */
/*----------------------------------------------------------------------------*/
void qfits_cache_purge(void);
/*----------------------------------------------------------------------------*/
/**
  @brief    Expand a keyword from shortFITS to HIERARCH notation.
  @param    keyword     Keyword to expand.
  @return   1 pointer to statically allocated string.

  This function expands a given keyword from shortFITS to HIERARCH
  notation, bringing it to uppercase at the same time.

  Examples:

  @verbatim
  det.dit          expands to     HIERARCH ESO DET DIT
  ins.filt1.id     expands to     HIERARCH ESO INS FILT1 ID
  @endverbatim

  If the input keyword is a regular FITS keyword (i.e. it contains
  not dots '.') the result is identical to the input.
 */
/*----------------------------------------------------------------------------*/
char * qfits_expand_keyword(char * keyword);
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
/*----------------------------------------------------------------------------*/
/**
  @brief    Compute the MD5 hash of data zones in a FITS file.
  @param    filename    Name of the FITS file to examine.
  @return   1 statically allocated character string, or NULL.

  This function expects the name of a FITS file.
  It will compute the MD5 hash on all data blocks in the main data section
  and possibly extensions (including zero-padding blocks if necessary) and
  return it as a string suitable for inclusion into a FITS keyword.

  The returned string is statically allocated inside this function,
  so do not free it or modify it. This function returns NULL in case
  of error.
 */
/*----------------------------------------------------------------------------*/
char * qfits_datamd5(char * filename);
/*-----------------------------------------------------------------------------
						Function ANSI C prototypes
 -----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/**
  @brief    Find the keyword in a key card (80 chars)   
  @param    line allocated 80-char line from a FITS header
  @return   statically allocated char *

  Find out the part of a FITS line corresponding to the keyword.
  Returns NULL in case of error. The returned pointer is statically
  allocated in this function, so do not modify or try to free it.
 */
/*----------------------------------------------------------------------------*/
char * qfits_getkey(char * line) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Find the value in a key card (80 chars) 
  @param    line allocated 80-char line from a FITS header
  @return   statically allocated char *

  Find out the part of a FITS line corresponding to the value.
  Returns NULL in case of error, or if no value can be found. The
  returned pointer is statically allocated in this function, so do not
  modify or try to free it.
 */
/*----------------------------------------------------------------------------*/
char * qfits_getvalue(char * line) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Find the comment in a key card (80 chars)   
  @param    line allocated 80-char line from a FITS header
  @return   statically allocated char *

  Find out the part of a FITS line corresponding to the comment.
  Returns NULL in case of error, or if no comment can be found. The
  returned pointer is statically allocated in this function, so do not
  modify or try to free it.
 */
/*----------------------------------------------------------------------------*/
char * qfits_getcomment(char * line) ;
/*----------------------------------------------------------------------------*/
/**
  @brief    Read a FITS header from a file to an internal structure.
  @param    filename    Name of the file to be read
  @return   Pointer to newly allocated qfits_header

  This function parses a FITS (main) header, and returns an allocated
  qfits_header object. The qfits_header object contains a linked-list of
  key "tuples". A key tuple contains:

  - A keyword
  - A value
  - A comment
  - An original FITS line (as read from the input file)

  Direct access to the structure is not foreseen, use accessor
  functions in fits_h.h

  Value, comment, and original line might be NULL pointers.
  The qfits_header type is an alias for the llist_t type from the list
  module, which should remain opaque.

  Returns NULL in case of error.
 */
/*----------------------------------------------------------------------------*/
qfits_header * qfits_header_read(char * filename) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Read an extension header from a FITS file.
  @param    filename    Name of the FITS file to read
  @param    xtnum       Extension number to read, starting from 0.
  @return   Newly allocated qfits_header structure.

  Strictly similar to qfits_header_read() but reads headers from
  extensions instead. If the requested xtension is 0, this function
  calls qfits_header_read() to return the main header.

  Returns NULL in case of error.
 */
/*----------------------------------------------------------------------------*/
qfits_header * qfits_header_readext(char * filename, int xtnum) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Pad an existing file with zeros to a multiple of 2880.
  @param    filename    Name of the file to pad.
  @return   void

  This function simply pads an existing file on disk with enough zeros
  for the file size to reach a multiple of 2880, as required by FITS.
 */
/*----------------------------------------------------------------------------*/
void qfits_zeropad(char * filename) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Identify if a file is a FITS file.
  @param    filename name of the file to check
  @return   int 0, 1, or -1

  Returns 1 if the file name looks like a valid FITS file. Returns
  0 else. If the file does not exist, returns -1.
 */
/*----------------------------------------------------------------------------*/
int is_fits_file(char *filename) ;
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
/*----------------------------------------------------------------------------*/
/**
  @brief	Test a floating-point variable for NaN value.
  @param    n   Number to test (float or double)
  @return   1 if n is NaN, 0 else.

  This macro is needed to support both float and double variables
  as input parameter. It checks on the size of the input variable
  to branch to the float or double version.

  Portability is an issue for this function which is present on
  most Unixes but not all, under various libraries (C lib on BSD,
  Math lib on Linux, sunmath on Solaris, ...). Integrating the
  code for this function makes qfits independent from any math
  library.
 */
/*----------------------------------------------------------------------------*/
#define qfits_isnan(n) ((sizeof(n)==sizeof(float)) ? _qfits_isnanf(n) : \
                        (sizeof(n)==sizeof(double)) ? _qfits_isnand(n) : -1)

/*----------------------------------------------------------------------------*/
/**
  @brief	Test a floating-point variable for Inf value.
  @param    n   Number to test (float or double)
  @return   1 if n is Inf or -Inf, 0 else.

  This macro is needed to support both float and double variables
  as input parameter. It checks on the size of the input variable
  to branch to the float or double version.

  Portability is an issue for this function which is missing on most
  Unixes. Most of the time, another function called finite() is
  offered to perform the opposite task, but it is not consistent
  among platforms and found in various libraries. Integrating the
  code for this function makes qfits independent from any math
  library.
 */
/*----------------------------------------------------------------------------*/
#define qfits_isinf(n) ((sizeof(n)==sizeof(float)) ? _qfits_isinff(n) : \
                        (sizeof(n)==sizeof(double)) ? _qfits_isinfd(n) : -1)

/*-----------------------------------------------------------------------------
   							Function prototypes
 -----------------------------------------------------------------------------*/
/**
 * Test a float variable for NaN value.
 * Do not call directly, call qfits_isnan().
 */
int _qfits_isnanf(float f);
/**
 * Test a float variable for Inf value.
 * Do not call directly, call qfits_isinf().
 */
int _qfits_isinff(float f);
/**
 * Test a double variable for NaN value.
 * Do not call directly, call qfits_isnan().
 */
int _qfits_isnand(double d);
/**
 * Test a double variable for Inf value.
 * Do not call directly, call qfits_isinf().
 */
int _qfits_isinfd(double d);

/*-----------------------------------------------------------------------------
  							Function prototypes
 -----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/**
  @brief    Open a new PAF file, output a default header.
  @param    filename    Name of the file to create.
  @param    paf_id      PAF identificator.
  @param    paf_desc    PAF description.
  @param    login_name  Login name
  @param    datetime    Date
  @return   Opened file pointer.

  This function creates a new PAF file with the requested file name.
  If another file already exists with the same name, it will be
  overwritten (if the file access rights allow it).

  A default header is produced according to the VLT DICB standard. You
  need to provide an identificator (paf_id) of the producer of the
  file. Typically, something like "ISAAC/zero_point".

  The PAF description (paf_desc) is meant for humans. Typically,
  something like "Zero point computation results".

  This function returns an opened file pointer, ready to receive more
  data through fprintf's. The caller is responsible for fclose()ing
  the file.
 */
/*----------------------------------------------------------------------------*/
FILE * qfits_paf_print_header(
        char    *   filename,
        char    *   paf_id,
        char    *   paf_desc,
        char    *   login_name,
        char    *   datetime) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Query a PAF file for a value.
  @param    filename    Name of the PAF to query.
  @param    key         Name of the key to query.
  @return   1 pointer to statically allocated string, or NULL.

  This function parses a PAF file and returns the value associated to a
  given key, as a pointer to an internal statically allocated string.
  Do not try to free or modify the contents of the returned string!

  If the key is not found, this function returns NULL.
 */
/*----------------------------------------------------------------------------*/
char * qfits_paf_query(
        char    *   filename,
        char    *   key) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    returns 1 if file is in PAF format, 0 else
  @param    filename name of the file to check
  @return   int 0, 1, or -1
  Returns 1 if the file name corresponds to a valid PAF file. Returns
  0 else. If the file does not exist, returns -1. Validity of the PAF file 
  is checked with the presence of PAF.HDR.START at the beginning
 */
/*----------------------------------------------------------------------------*/
int qfits_is_paf_file(char * filename) ;

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

/*----------------------------------------------------------------------------*/
/**
  @brief	Get the current status of error display.
  @return	int 1 if error display is active, 0 if not.

  This function returns the current error display status. If it returns 1,
  it means that all calls to qfits_error/qfits_warning will display
  messages using the registered functions, otherwise they do nothing.
 */
/*----------------------------------------------------------------------------*/
int qfits_err_statget(void);

/*----------------------------------------------------------------------------*/
/**
  @brief	Set the current status of error display.
  @param	sta		New status to be set.
  @return	int giving the previous display status.

  This function sets the current error display status to the required
  value, and returns the previous value. It is useful to store the
  previous value, in view of restoring it afterwards, e.g. to make a
  function silent on all its errors. Example:

  \begin{verbatim}
  int prev_stat = qfits_err_statset(0) ;
  function_call() ;
  qfits_err_statset(prev_stat);
  \end{verbatim}
 */
/*----------------------------------------------------------------------------*/
int qfits_err_statset(int sta);

/*----------------------------------------------------------------------------*/
/**
  @brief	Register a function to display error/warning messages.
  @param	dispfn	Display function (see doc below).
  @return	int 0 if function was registered, -1 if not.

  This function registers a display function into the error-handling
  module. Display functions have the following prototype:

  @code
  void display_function(char * msg);
  @endcode

  They are simple functions that expect a ready-made error message
  and return void. They can do whatever they want with the message
  (log it to a file, send it to a GUI, to the syslog, ...). The
  message is built using a printf-like statement in qfits_error and
  qfits_warning, then passed to all registered display functions.

  A maximum of QFITS_ERR_MAXERRDISP can be registered (see source code).
  If the limit has been reached, this function will signal it by
  returning -1.
 */
/*----------------------------------------------------------------------------*/
int qfits_err_register( void (*dispfn)(char*) ) ;
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

/*-----------------------------------------------------------------------------
   								Defines
 -----------------------------------------------------------------------------*/

/** Maximum supported file name size */
#define FILENAMESZ				512

/** Maximum number of characters per line in an ASCII file */
#define ASCIILINESZ				1024

/*-----------------------------------------------------------------------------
						Function ANSI C prototypes
 -----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/**
  @brief    Returns the current date as a long (CCYYMMDD).
  @return   The current date as a long number.

  Returns the current date as a long value (CCYYMMDD). Since most
  system clocks do not return a century, this function assumes that
  all years 80 and above are in the 20th century, and all years 00 to
  79 are in the 21st century.  For best results, consume before 1 Jan
  2080.

  Example:  19 Oct 2000 is returned as 20001019
 */
/*----------------------------------------------------------------------------*/
long date_now (void);

/*----------------------------------------------------------------------------*/
/**
  @brief    Returns the current time as a long (HHMMSSCC).
  @return   The current time as a long number.

  Returns the current time as a long value (HHMMSSCC). If the system
  clock does not return centiseconds, these are set to zero.
  Example: 15:36:12.84 is returned as 15361284
 */
/*----------------------------------------------------------------------------*/
long time_now(void);

/*----------------------------------------------------------------------------*/
/**
  @brief    Returns the current date as a static string.
  @return   Pointer to statically allocated string.
 
  Build and return a string containing the date of today in ISO8601
  format. The returned pointer points to a statically allocated string
  in the function, so no need to free it.
  */
/*----------------------------------------------------------------------------*/
char * get_date_iso8601(void);

/*----------------------------------------------------------------------------*/
/**
  @brief    Returns the current date and time as a static string.
  @return   Pointer to statically allocated string
 
  Build and return a string containing the date of today and the
  current time in ISO8601 format. The returned pointer points to a
  statically allocated string in the function, so no need to free it.
 */
/*----------------------------------------------------------------------------*/
char * get_datetime_iso8601(void);

/*-----------------------------------------------------------------------------
   								Defines
 -----------------------------------------------------------------------------*/

/* The following defines the maximum acceptable size for a FITS value */
#define FITSVALSZ					60

#define QFITS_INVALIDTABLE			0
#define QFITS_BINTABLE				1
#define QFITS_ASCIITABLE			2

/*-----------------------------------------------------------------------------
   								New types
 -----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/**
  @brief    Column data type
 */ 
/*----------------------------------------------------------------------------*/
typedef enum _TFITS_DATA_TYPE_ {
	TFITS_ASCII_TYPE_A,
	TFITS_ASCII_TYPE_D,
	TFITS_ASCII_TYPE_E,
	TFITS_ASCII_TYPE_F,
	TFITS_ASCII_TYPE_I,
	TFITS_BIN_TYPE_A,
	TFITS_BIN_TYPE_B,
	TFITS_BIN_TYPE_C,
	TFITS_BIN_TYPE_D,
	TFITS_BIN_TYPE_E,
	TFITS_BIN_TYPE_I,
	TFITS_BIN_TYPE_J,
	TFITS_BIN_TYPE_L,
	TFITS_BIN_TYPE_M,
	TFITS_BIN_TYPE_P,
	TFITS_BIN_TYPE_X,
	TFITS_BIN_TYPE_UNKNOWN
} tfits_type ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Column object

  This structure contains all information needed to read a column in a table.
  These informations come from the header. 
  The qfits_table object contains a list of qfits_col objects.

  This structure has to be created from scratch and filled if one want to 
  generate a FITS table.
 */
/*----------------------------------------------------------------------------*/
typedef struct qfits_col
{
	/** 
	  Number of atoms in one field.
	 In ASCII tables, it is the number of characters in the field as defined
	 in TFORM%d keyword.
	 In BIN tables, it is the number of atoms in each field. For type 'A', 
	 it is the number of characters. A field with two complex object will
	 have atom_nb = 4.
	*/
	int			atom_nb ;

    /**
     Number of decimals in a ASCII field. 
     This value is always 0 for BIN tables
    */
    int         atom_dec_nb ;

	/** 
	  Size of one element in bytes. In ASCII tables, atom_size is the size
	  of the element once it has been converted in its 'destination' type.
	  For example, if "123" is contained in an ASCII table in a column 
	  defined as I type, atom_nb=3, atom_size=4.
	  In ASCII tables:
	   - type 'A' : atom_size = atom_nb = number of chars
	   - type 'I', 'F' or 'E' : atom_size = 4
	   - type 'D' : atom_size = 8
	  In BIN tables :
	   - type 'A', 'L', 'X', 'B': atom_size = 1 
	   - type 'I' : atom_size = 2
	   - type 'E', 'J', 'C', 'P' : atom_size = 4
	   - type 'D', 'M' : atom_size = 8
	  In ASCII table, there is one element per field. The size in bytes and 
	  in number of characters is atom_nb, and the size in bytes after 
	  conversion of the field is atom_size.
	  In BIN tables, the size in bytes of a field is always atom_nb*atom_size.
	 */
	int			atom_size ;	
	
	/** 
	  Type of data in the column as specified in TFORM keyword 
	  In ASCII tables : TFITS_ASCII_TYPE_* with *=A, I, F, E or D 
	  In BIN tables : TFITS_BIN_TYPE_* with *=L, X, B, I, J, A, E, D, C, M or P 
	*/
	tfits_type	atom_type ;

	/** Label of the column */
	char    	tlabel[FITSVALSZ] ;

	/** Unit of the data */
	char    	tunit[FITSVALSZ] ;
	
	/** Null value */
	char		nullval[FITSVALSZ] ;

	/** Display format */
	char		tdisp[FITSVALSZ] ;
	
	/** 
	  zero and scale are used when the quantity in the field does not	 
	  represent a true physical quantity. Basically, thez should be used
	  when they are present: physical_value = zero + scale * field_value 
	  They are read from TZERO and TSCAL in the header
	 */
	int			zero_present ;	
	float		zero ;        
	int			scale_present ;
	float		scale ;   

	/** Offset between the beg. of the table and the beg. of the column.  */
    int			off_beg ;
	
	/** Flag to know if the column is readable. An empty col is not readable */
	int			readable ;
} qfits_col ;


/*----------------------------------------------------------------------------*/
/**
  @brief    Table object

  This structure contains all information needed to read a FITS table.
  These information come from the header. The object is created by 
  qfits_open().
 
  To read a FITS table, here is a code example:
  @code
  int main(int argc, char* argv[])
  {
  	qfits_table     *   table ;
 	int					n_ext ;
	int					i ;

	// Query the number of extensions
	n_ext = qfits_query_n_ext(argv[1]) ;
	
	// For each extension
	for (i=0 ; i<n_ext ; i++) {
		// Read all the infos about the current table 
		table = qfits_table_open(argv[1], i+1) ;
		// Display the current table 
		dump_extension(table, stdout, '|', 1, 1) ;
	}
	return ;
  }
  @endcode
 */
/*----------------------------------------------------------------------------*/
typedef struct qfits_table
{
	/**
		Name of the file the table comes from or it is intended to end to
	 */
	char			filename[FILENAMESZ] ;
	/** 
		Table type. 
        Possible values: QFITS_INVALIDTABLE, QFITS_BINTABLE, QFITS_ASCIITABLE
	 */
	int				tab_t ;
	/** Width in bytes of the table */
	int				tab_w ;			
	/** Number of columns */
	int				nc ;			
	/** Number of raws */
	int			    nr ;
	/** Array of qfits_col objects */
	qfits_col	*	col ;			
} qfits_table ;

/*-----------------------------------------------------------------------------
   							Function prototypes
 -----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/**
  @brief    Identify a file as containing a FITS table in extension.
  @param    filename    Name of the FITS file to examine.
  @param    xtnum       Extension number to check (starting from 1).
  @return   int 1 if the extension contains a table, 0 else.

  Examines the requested extension and identifies the presence of a
  FITS table.
 */
/*----------------------------------------------------------------------------*/
int qfits_is_table(char * filename, int xtnum) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Generate a default primary header to store tables   
  @return   the header object   
 */
/*----------------------------------------------------------------------------*/
qfits_header * qfits_table_prim_header_default(void) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Generate a default extension header to store tables   
  @return   the header object   
 */
/*----------------------------------------------------------------------------*/
qfits_header * qfits_table_ext_header_default(qfits_table *) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Table object constructor
  @param    filename    Name of the FITS file associated to the table
  @param    table_type  Type of the table (QFITS_ASCIITABLE or QFITS_BINTABLE)
  @param    table_width Width in bytes of the table
  @param    nb_cols     Number of columns
  @param    nb_raws     Number of raws
  @return   The table object
  The columns are also allocated. The object has to be freed with 
  qfits_table_close()
 */
/*----------------------------------------------------------------------------*/
qfits_table * qfits_table_new(
        char    *   filename,
        int         table_type,
        int         table_width,
        int         nb_cols,
        int         nb_raws) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Fill a column object with some provided informations
  @param    qc      Pointer to the column that has to be filled
  @param    unit    Unit of the data 
  @param    label   Label of the column 
  @param    disp    Way to display the data 
  @param    nullval Null value
  @param    atom_nb Number of atoms per field. According to the type, an atom 
                    is a double, an int, a char, ... 
  @param    atom_dec_nb Number of decimals as specified in TFORM 
  @param    atom_size   Size in bytes of the field for ASCII tables, and of 
                        an atom for BIN tables. ASCII tables only contain 1 
                        atom per field (except for A type where you can of
                        course have more than one char per field)
  @param    atom_type   Type of data (11 types for BIN, 5 for ASCII)
  @param    zero_present    Flag to use or not zero
  @param    zero            Zero value
  @param    scale_present   Flag to use or not scale
  @param    scale           Scale value
  @param    offset_beg  Gives the position of the column
  @return   -1 in error case, 0 otherwise
 */
/*----------------------------------------------------------------------------*/
int qfits_col_fill(
        qfits_col   *   qc,
        int             atom_nb,
        int             atom_dec_nb,
        int             atom_size,
        tfits_type      atom_type,
        char        *   label,
        char        *   unit,
        char        *   nullval,
        char        *   disp,
        int             zero_present,
        float           zero,
        int             scale_present,
        float           scale,
        int             offset_beg) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Read a FITS extension.
  @param    filename    Name of the FITS file to examine.
  @param    xtnum       Extension number to read (starting from 1).
  @return   Pointer to newly allocated qfits_table structure.

  Read a FITS table from a given file name and extension, and return a
  newly allocated qfits_table structure. 
 */
/*----------------------------------------------------------------------------*/
qfits_table * qfits_table_open(
        char    *   filename,
        int         xtnum) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Free a FITS table and associated pointers
  @param    t qfits_table to free
  @return   void
  Frees all memory associated to a qfits_table structure.
 */
/*----------------------------------------------------------------------------*/
void qfits_table_close(qfits_table * t) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Extract data from a column in a FITS table
  @param    th      Allocated qfits_table
  @param    colnum  Number of the column to extract (from 0 to colnum-1)
  @param    selection  boolean array to define the selected rows
  @return   unsigned char array

  If selection is NULL, select the complete column.
  
  Extract a column from a FITS table and return the data as a bytes 
  array. The returned array type and size are determined by the
  column object in the qfits_table and by the selection parameter.

  Returned array size in bytes is:
  nbselected * col->natoms * col->atom_size

  Numeric types are correctly understood and byte-swapped if needed,
  to be converted to the local machine type.

  NULL values have to be handled by the caller.

  The returned buffer has been allocated using one of the special memory
  operators present in xmemory.c. To deallocate the buffer, you must call
  the version of free() offered by xmemory, not the usual system free(). It
  is enough to include "xmemory.h" in your code before you make calls to 
  the pixel loader here.
 */
/*----------------------------------------------------------------------------*/
unsigned char * qfits_query_column(
        qfits_table     *   th,
        int                 colnum,
        int             *   selection) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Extract consequtive values from a column in a FITS table
  @param    th      Allocated qfits_table
  @param    colnum  Number of the column to extract (from 0 to colnum-1)
  @param    start_ind   Index of the first row (0 for the first)
  @param    nb_rows     Number of rows to extract
  @return   unsigned char array
  Does the same as qfits_query_column() but on a consequtive sequence of rows
  Spares the overhead of the selection object allocation
 */
/*----------------------------------------------------------------------------*/
unsigned char * qfits_query_column_seq(
        qfits_table     *   th,
        int                 colnum,
        int                 start_ind,
        int                 nb_rows) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Compute the table width in bytes from the columns infos 
  @param    th      Allocated qfits_table
  @return   the width (-1 in error case)
 */
/*----------------------------------------------------------------------------*/
int qfits_compute_table_width(qfits_table * th) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Extract binary data from a column in a FITS table
  @param    th      Allocated qfits_table
  @param    colnum  Number of the column to extract (from 0 to colnum-1)
  @param    selection  bollean array to identify selected rows
  @param    null_value  Value to return when a NULL value comes
  @return   Pointer to void *

  Extract a column from a FITS table and return the data as a generic
  void* array. The returned array type and size are determined by the
  column object in the qfits_table.
    
  Returned array size in bytes is:
  nb_selected * col->atom_nb * col->atom_size
  
  NULL values are recognized and replaced by the specified value.

  The returned buffer has been allocated using one of the special memory
  operators present in xmemory.c. To deallocate the buffer, you must call
  the version of free() offered by xmemory, not the usual system free(). It
  is enough to include "xmemory.h" in your code before you make calls to 
  the pixel loader here.
 */
/*----------------------------------------------------------------------------*/
void * qfits_query_column_data(
        qfits_table     *   th,
        int                 colnum,
        int             *   selection,
        void            *   null_value) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Extract binary data from a column in a FITS table
  @param    th      Allocated qfits_table
  @param    colnum  Number of the column to extract (from 0 to colnum-1)
  @param    start_ind   Index of the first row (0 for the first)
  @param    nb_rows     Number of rows to extract
  @param    null_value  Value to return when a NULL value comes
  @return   Pointer to void *
  Does the same as qfits_query_column_data() but on a consequtive sequence 
  of rows.  Spares the overhead of the selection object allocation
 */
/*----------------------------------------------------------------------------*/
void * qfits_query_column_seq_data(
        qfits_table     *   th,
        int                 colnum,
        int                 start_ind,
        int                 nb_rows,
        void            *   null_value) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Detect NULL values in a column
  @param    th      Allocated qfits_table
  @param    colnum  Number of the column to check (from 0 to colnum-1)
  @param    selection Array to identify selected rows
  @param    nb_vals Gives the size of the output array
  @param    nb_nulls Gives the number of detected null values
  @return   array with 1 for NULLs and 0 for non-NULLs  
 */
/*----------------------------------------------------------------------------*/
int * qfits_query_column_nulls(
        qfits_table     *   th,
        int                 colnum,
        int             *   selection,
        int             *   nb_vals,
        int             *   nb_nulls) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Save a table to a FITS file with a given FITS header.
  @param    array           Data array.
  @param    table           table
  @param    fh              FITS header to insert in the output file.
  @return   -1 in error case, 0 otherwise
 */
/*----------------------------------------------------------------------------*/
int qfits_save_table_hdrdump(
        void            **  array,
        qfits_table     *   table,
        qfits_header    *   fh) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Appends a std extension header + data to a FITS table file.
  @param    outfile     Pointer to (opened) file ready for writing.
  @param    t           Pointer to qfits_table
  @param    data        Table data to write
  @return   int 0 if Ok, -1 otherwise

  Dumps a FITS table to a file. The whole table described by qfits_table, and
  the data arrays contained in 'data' are dumped to the file. An extension
  header is produced with all keywords needed to describe the table, then the
  data is dumped to the file.
  The output is then padded to reach a multiple of 2880 bytes in size.
  Notice that no main header is produced, only the extension part.
 */
/*----------------------------------------------------------------------------*/
int qfits_table_append_xtension(
        FILE            *   outfile,
        qfits_table     *   t,
        void            **  data) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Appends a specified extension header + data to a FITS table file.
  @param    outfile     Pointer to (opened) file ready for writing.
  @param    t           Pointer to qfits_table
  @param    data        Table data to write
  @param    hdr         Specified extension header
  @return   int 0 if Ok, -1 otherwise

  Dumps a FITS table to a file. The whole table described by qfits_table, and
  the data arrays contained in 'data' are dumped to the file following the
  specified fits header.
  The output is then padded to reach a multiple of 2880 bytes in size.
  Notice that no main header is produced, only the extension part.
 */
/*----------------------------------------------------------------------------*/
int qfits_table_append_xtension_hdr(
        FILE            *   outfile,
        qfits_table     *   t,
        void            **  data,
        qfits_header    *   hdr) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    given a col and a row, find out the string to write for display
  @param    table   table structure
  @param    col_id  col id (0 -> nbcol-1)
  @param    row_id  row id (0 -> nrow-1)
  @param    use_zero_scale  Flag to use or not zero and scale
  @return   the string to write
 */
/*----------------------------------------------------------------------------*/
char * qfits_table_field_to_string(
        qfits_table     *   table,
        int                 col_id,
        int                 row_id,
        int                 use_zero_scale) ;



#endif

