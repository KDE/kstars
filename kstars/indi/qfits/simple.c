/*----------------------------------------------------------------------------*/
/**
   @file    simple.c
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

/*-----------------------------------------------------------------------------
                                Includes
 -----------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <regex.h>

#include "simple.h"
#include "fits_p.h"
#include "expkey.h"
#include "cache.h"
#include "fits_std.h"
#include "qerror.h"
#include "xmemory.h"

/*-----------------------------------------------------------------------------
                            Global variables
 -----------------------------------------------------------------------------*/

/*
 * The following global variables are only used for regular expression
 * matching of integers and floats. These definitions are private to
 * this module.
 */
/** A regular expression matching a floating-point number */
static char regex_float[] =
    "^[+-]?([0-9]+[.]?[0-9]*|[.][0-9]+)([eE][+-]?[0-9]+)?$";

/** A regular expression matching an integer */
static char regex_int[] = "^[+-]?[0-9]";

/** A regular expression matching a complex number (int or float) */
static char regex_cmp[] =
"^[+-]?([0-9]+[.]?[0-9]*|[.][0-9]+)([eE][+-]?[0-9]+)?[ ]+[+-]?([0-9]+[.]?[0-9]*|[.][0-9]+)([eE][+-]?[0-9]+)?$";

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
char * qfits_query_hdr(char * filename, char * keyword)
{
    return qfits_query_ext(filename, keyword, 0);
}

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
char * qfits_query_ext(char * filename, char * keyword, int xtnum)
{
    char    *   exp_key ;
    char    *   where ;
    char    *   start ;
    char    *   value ;
    char        test1, test2 ;
    int         i ;
    int         len ;
    int         different ;
    int         seg_start ;
    int         seg_size ;
    long        bufcount ;

    /* Bulletproof entries */
    if (filename==NULL || keyword==NULL || xtnum<0) return NULL ;

    /* Expand keyword */
    exp_key = qfits_expand_keyword(keyword);

    /*
     * Find out offsets to the required extension
     * Record the xtension start and stop offsets
     */
    if (qfits_get_hdrinfo(filename, xtnum, &seg_start, &seg_size)==-1) {
        return NULL ;
    }

    /*
     * Get a hand on requested buffer
     */

    start = falloc(filename, seg_start, NULL);
    if (start==NULL) return NULL ;

    /*
     * Look for keyword in header
     */

    bufcount=0 ;
    where = start ;
    len = (int)strlen(exp_key);
    while (1) {
        different=0 ;
        for (i=0 ; i<len ; i++) {
            if (where[i]!=exp_key[i]) {
                different++ ;
                break ;
            }
        }
        if (!different) {
            /* Get 2 chars after keyword */
            test1=where[len];
            test2=where[len+1];
            /* If first subsequent character is the equal sign, bingo. */
            if (test1=='=') break ;
            /* If subsequent char is equal sign, bingo */
            if (test1==' ' && (test2=='=' || test2==' '))
                break ;
        }
        /* Watch out for header end */
        if ((where[0]=='E') &&
            (where[1]=='N') &&
            (where[2]=='D') &&
            (where[3]==' ')) {
            /* Detected header end */
            free(start);
            return NULL ;
        }
        /* Forward one line */
        where += 80 ;
        bufcount += 80 ;
        if (bufcount>seg_size) {
            /* File is damaged or not FITS: bailout */
            free(start);
            return NULL ;
        }
    }

    /* Found the keyword, now get its value */
    value = qfits_getvalue(where);
    free(start);
    return value;
}

/*----------------------------------------------------------------------------*/
/**
  @brief    Counts the number of extensions in a FITS file
  @param    filename    Name of the FITS file to browse.
  @return   int
  Counts how many extensions are in the file. Returns 0 if no
  extension is found, and -1 if an error occurred.
 */
/*----------------------------------------------------------------------------*/
int qfits_query_n_ext(char * filename)
{
    return qfits_query(filename, QFITS_QUERY_N_EXT);
}

/*----------------------------------------------------------------------------*/
/**
  @brief    Counts the number of planes in a FITS extension.
  @param    filename    Name of the FITS file to browse.
  @param	exnum		Extensin number
  @return   int
  Counts how many planes are in the extension. Returns 0 if no plane is found, 
  and -1 if an error occurred.
 */
/*----------------------------------------------------------------------------*/
int qfits_query_nplanes(char * filename, int extnum)
{
	char	*	sval ;
	int			next ;
	int			naxes ;
	int			nplanes ;

	/* Check file existence */
    if (filename == NULL) return -1 ;
	/* Check validity of extnum */
	next = qfits_query_n_ext(filename) ;
	if (extnum>next) {
		qfits_error("invalid extension specified") ;
		return -1 ;
	}

    /* Find the number of axes  */
    naxes = 0 ;
    if ((sval = qfits_query_ext(filename, "NAXIS", extnum)) == NULL) {
        qfits_error("missing key in header: NAXIS");
		return -1 ;
	}
	naxes = atoi(sval) ;

	/* Check validity of naxes */
    if ((naxes < 2) || (naxes > 3)) return -1 ;

	/* Two dimensions cube */
	if (naxes == 2) nplanes = 1 ;
	else {
		/* For 3D cubes, get the third dimension size   */
		if ((sval = qfits_query_ext(filename, "NAXIS3", extnum))==NULL) {
			qfits_error("missing key in header: NAXIS3");
			return -1 ;
		}
		nplanes = atoi(sval);
        if (nplanes < 1) nplanes = 0 ;
	}
    return nplanes ;
}

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
#define PRETTY_STRING_STATICBUFS    8
char * qfits_pretty_string(char * s)
{
    static char     pretty_buf[PRETTY_STRING_STATICBUFS][81] ;
    static int      flip=0 ;
    char        *   pretty ;
    int             i,j ;

    /* bulletproof */
    if (s==NULL) return NULL ;

    /* Switch between static buffers */
    pretty = pretty_buf[flip];
    flip++ ;
    if (flip==PRETTY_STRING_STATICBUFS)
        flip=0 ;
    
    pretty[0] = (char)0 ;
    if (s[0]!='\'') return s ;

    /* skip first quote */
    i=1 ;
    j=0 ;
    /* trim left-side blanks */
    while (s[i]==' ') {
        if (i==(int)strlen(s)) break ;
        i++ ;
    }
    if (i>=(int)(strlen(s)-1)) return pretty ;
    /* copy string, changing double quotes to single ones */
    while (i<(int)strlen(s)) {
        if (s[i]=='\'') {
            i++ ;
        }
        pretty[j]=s[i];
        i++ ;
        j++ ;
    }
    /* NULL-terminate the pretty string */
    pretty[j+1]=(char)0;
    /* trim right-side blanks */
    j = (int)strlen(pretty)-1;
    while (pretty[j]==' ') j-- ;
    pretty[j+1]=(char)0;
    return pretty;
}
#undef PRETTY_STRING_STATICBUFS

/*----------------------------------------------------------------------------*/
/**
  @brief    Identify if a FITS value is boolean
  @param    s FITS value as a string
  @return   int 0 or 1

  Identifies if a FITS value is boolean.
 */
/*----------------------------------------------------------------------------*/
int qfits_is_boolean(char * s)
{
    if (s==NULL) return 0 ;
    if (s[0]==0) return 0 ;
    if ((int)strlen(s)>1) return 0 ;
    if (s[0]=='T' || s[0]=='F') return 1 ;
    return 0 ;
}

/*----------------------------------------------------------------------------*/
/**
  @brief    Identify if a FITS value is an int.
  @param    s FITS value as a string
  @return   int 0 or 1

  Identifies if a FITS value is an integer.
 */
/*----------------------------------------------------------------------------*/
int qfits_is_int(char * s)
{
    regex_t re_int ;
    int     status ;

    if (s==NULL) return 0 ;
    if (s[0]==0) return 0 ;
    
    if (regcomp(&re_int, "^[+-]?[0-9]", REG_EXTENDED|REG_NOSUB)!=0) {
        qfits_error("internal error: compiling int rule");
        exit(-1);
    }
    status = regexec(&re_int, s, 0, NULL, 0) ;
    regfree(&re_int) ; 
    return (status) ? 0 : 1 ;
}

/*----------------------------------------------------------------------------*/
/**
  @brief    Identify if a FITS value is float.
  @param    s FITS value as a string
  @return   int 0 or 1

  Identifies if a FITS value is float.
 */
/*----------------------------------------------------------------------------*/
int qfits_is_float(char * s)
{
    regex_t re_float;
    int     status ;

    if (s==NULL) return 0 ;
    if (s[0]==0) return 0 ;
    if (regcomp(&re_float, &regex_float[0], REG_EXTENDED|REG_NOSUB)!=0) {
        qfits_error("internal error: compiling float rule");
        exit(-1);
    }
    status = regexec(&re_float, s, 0, NULL, 0) ;
    regfree(&re_float) ; 
    return (status) ? 0 : 1 ;
}

/*----------------------------------------------------------------------------*/
/**
  @brief    Identify if a FITS value is complex.
  @param    s FITS value as a string
  @return   int 0 or 1

  Identifies if a FITS value is complex.
 */
/*----------------------------------------------------------------------------*/
int qfits_is_complex(char * s)
{
    regex_t re_cmp ;
    int     status ;

    if (s==NULL) return 0 ;
    if (s[0]==0) return 0 ;
    if (regcomp(&re_cmp, &regex_cmp[0], REG_EXTENDED|REG_NOSUB)!=0) {
        qfits_error("internal error: compiling complex rule");
        exit(-1);
    }
    status = regexec(&re_cmp, s, 0, NULL, 0) ;
    regfree(&re_cmp) ; 
    return (status) ? 0 : 1 ;
}

/*----------------------------------------------------------------------------*/
/**
  @brief    Identify if a FITS value is string.
  @param    s FITS value as a string
  @return   int 0 or 1

  Identifies if a FITS value is a string.
 */
/*----------------------------------------------------------------------------*/
int qfits_is_string(char * s)
{
    if (s==NULL) return 0 ;
    if (s[0]=='\'') return 1 ;
    return 0 ;
}

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
int qfits_get_type(char * s)
{
    if (qfits_is_boolean(s)) return QFITS_BOOLEAN ;
    if (qfits_is_int(s)) return QFITS_INT ;
    if (qfits_is_float(s)) return QFITS_FLOAT ;
    if (qfits_is_complex(s)) return QFITS_COMPLEX ;
    if (qfits_is_string(s)) return QFITS_STRING ;
    return QFITS_UNKNOWN ;
}

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
        int  * seg_size)
{
    if (filename==NULL || xtnum<0 || (seg_start==NULL && seg_size==NULL)) {
        return -1 ;
    }
    if (seg_start!=NULL) {
        *seg_start = qfits_query(filename, QFITS_QUERY_HDR_START | xtnum);
        if (*seg_start<0)
            return -1 ;
    }
    if (seg_size!=NULL) {
        *seg_size = qfits_query(filename, QFITS_QUERY_HDR_SIZE | xtnum);
        if (*seg_size<0)
            return -1 ;
    }
    return 0 ;
}

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
        int  * seg_size)
{
    if (filename==NULL || xtnum<0 || (seg_start==NULL && seg_size==NULL)) {
        return -1 ;
    }
    if (seg_start!=NULL) {
        *seg_start = qfits_query(filename, QFITS_QUERY_DAT_START | xtnum);
        if (*seg_start<0)
            return -1 ;
    }
    if (seg_size!=NULL) {
        *seg_size = qfits_query(filename, QFITS_QUERY_DAT_SIZE | xtnum);
        if (*seg_size<0)
            return -1 ;
    }
    return 0 ;
}

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
        char    *   keyword) 
{
    char    *   exp_key ;
    int         fd ;
    char    *   buf ;
    char    *   buf2 ;
    char    *   where ;
    int         hs ;
    char    *   card ;

    /* Bulletproof entries */
    if (filename==NULL || keyword==NULL) return NULL ;

    /* Expand keyword */
    exp_key = qfits_expand_keyword(keyword) ;

    /* Memory-map the FITS header of the input file  */
    qfits_get_hdrinfo(filename, 0, NULL, &hs) ;
    if (hs < 1) {
        qfits_error("error getting FITS header size for %s", filename);
        return NULL ;
    }
    fd = open(filename, O_RDWR) ;
    if (fd == -1) return NULL ;
    buf = (char*)mmap(0,
                      hs,
                      PROT_READ | PROT_WRITE,
                      MAP_SHARED,
                      fd,
                      0) ;
    if (buf == (char*)-1) {
        perror("mmap") ;
        close(fd) ;
        return NULL ;
    }

    /* Apply search for the input keyword */
    buf2 = malloc(hs+1) ;
    memcpy(buf2, buf, hs) ;
    buf2[hs] = (char)0 ;
    where = buf2 ;
    do {
        where = strstr(where, exp_key);
        if (where == NULL) {
            close(fd);
            munmap(buf,hs);
            free(buf2) ;
            return NULL ;
        }
        if ((where-buf2)%80) where++ ;
    } while ((where-buf2)%80) ;
       
    where = buf + (int)(where - buf2) ;
  
    /* Create the card */
    card = malloc(81*sizeof(char)) ;
    strncpy(card, where, 80) ;
    card[80] = (char)0 ;

    /* Free and return */
    close(fd) ;
    munmap(buf, hs) ;
    free(buf2) ;
    return card ;
}

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
        char    *   substitute)
{
    char    *   exp_key ;
    int         fd ;
    char    *   buf ;
    char    *   buf2 ;
    char    *   where ;
    int         hs ;


    /* Bulletproof entries */
    if (filename==NULL || keyword==NULL || substitute==NULL) return -1 ;

    /* Expand keyword */
    exp_key = qfits_expand_keyword(keyword);
    /*
     * Memory-map the FITS header of the input file 
     */

    qfits_get_hdrinfo(filename, 0, NULL, &hs) ;
    if (hs < 1) {
        qfits_error("error getting FITS header size for %s", filename);
        return -1 ;
    }
    fd = open(filename, O_RDWR) ;
    if (fd == -1) {
        return -1 ;
    }
    buf = (char*)mmap(0,
                      hs,
                      PROT_READ | PROT_WRITE,
                      MAP_SHARED,
                      fd,
                      0) ;
    if (buf == (char*)-1) {
        perror("mmap") ;
        close(fd) ;
        return -1 ;
    }

    /* Apply search and replace for the input keyword lists */
    buf2 = malloc(hs+1) ;
    memcpy(buf2, buf, hs) ;
    buf2[hs] = (char)0 ;
    where = buf2 ;
    do {
        where = strstr(where, exp_key);
        if (where == NULL) {
            close(fd);
            munmap(buf,hs);
            free(buf2) ;
            return -1 ;
        }
        if ((where-buf2)%80) where++ ;
    } while ((where-buf2)%80) ;
       
    where = buf + (int)(where - buf2) ;
    
    /* Replace current placeholder by blanks */
    memset(where, ' ', 80);
    /* Copy substitute into placeholder */
    memcpy(where, substitute, strlen(substitute));

    close(fd) ;
    munmap(buf, hs) ;
    free(buf2) ;
    return 0 ;
}

/* vim: set ts=4 et sw=4 tw=75 */
