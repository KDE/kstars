/*----------------------------------------------------------------------------*/
/**
   @file    fits_rw.c
   @author  N. Devillard
   @date    Mar 2000
   @version $Revision$
   @brief   FITS header reading/writing.
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
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "fits_rw.h"
#include "fits_h.h"
#include "fits_p.h"
#include "simple.h"
#include "xmemory.h"
#include "qerror.h"

/*-----------------------------------------------------------------------------
                        Private to this module  
 -----------------------------------------------------------------------------*/
static int is_blank_line(char * s)
{
    int     i ;

    for (i=0 ; i<(int)strlen(s) ; i++) {
        if (s[i]!=' ') return 0 ;
    }
    return 1 ;
}

/*-----------------------------------------------------------------------------
                            Function codes
 -----------------------------------------------------------------------------*/

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
qfits_header * qfits_header_read(char * filename)
{
    /* Forward job to readext */
    return qfits_header_readext(filename, 0);
}

/*----------------------------------------------------------------------------*/
/**
  @brief    Read an extension header from a FITS file.
  @param    filename    Name of the FITS file to read
  @param    xtnum       Extension number to read, starting from 0.
  @return   Newly allocated qfits_header structure.

  Strictly similar to qfits_header_read() but reads headers from
  extensions instead. If the requested xtension is 0, this function
  returns the main header.

  Returns NULL in case of error.
 */
/*----------------------------------------------------------------------------*/
qfits_header * qfits_header_readext(char * filename, int xtnum)
{
    qfits_header*   hdr ;
    int             n_ext ;
    char            line[81];
    char        *   where ;
    char        *   start ;
    char        *   key,
                *   val,
                *   com ;
    int             seg_start ;
    int             seg_size ;

    /* Check input */
    if (filename==NULL || xtnum<0)
        return NULL ;

    /* Check that there are enough extensions */
    if (xtnum>0) {
        n_ext = qfits_query_n_ext(filename);
        if (xtnum>n_ext) {
            return NULL ;
        }
    }

    /* Get offset to the extension header */
    if (qfits_get_hdrinfo(filename, xtnum, &seg_start, &seg_size)!=0) {
        return NULL ;
    }

    /* Memory-map the input file */
    start = falloc(filename, seg_start, NULL) ;
    if (start==NULL)
        return NULL ;

    hdr   = qfits_header_new() ;
    where = start ;
    while (1) {
        memcpy(line, where, 80);
        line[80] = (char)0;

        /* Rule out blank lines */
        if (!is_blank_line(line)) {

            /* Get key, value, comment for the current line */
            key = qfits_getkey(line);
            val = qfits_getvalue(line);
            com = qfits_getcomment(line);

            /* If key or value cannot be found, trigger an error */
            if (key==NULL) {
                qfits_header_destroy(hdr);
                hdr = NULL ;
                break ;
            }
            /* Append card to linked-list */
            qfits_header_append(hdr, key, val, com, line);
            /* Check for END keyword */
            if (strlen(key)==3)
                if (key[0]=='E' &&
                    key[1]=='N' &&
                    key[2]=='D')
                    break ;
        }
        where += 80 ;
        /* If reaching the end of file, trigger an error */
        if ((int)(where-start)>=(int)(seg_size+80)) {
            qfits_header_destroy(hdr);
            hdr = NULL ;
            break ;
        }
    }
    free(start);
    return hdr ;
}

/*----------------------------------------------------------------------------*/
/**
  @brief    Pad an existing file with zeros to a multiple of 2880.
  @param    filename    Name of the file to pad.
  @return   void

  This function simply pads an existing file on disk with enough zeros
  for the file size to reach a multiple of 2880, as required by FITS.
 */
/*----------------------------------------------------------------------------*/
void qfits_zeropad(char * filename)
{
    struct stat sta ;
    int         size ;
    int         remaining;
    FILE    *   out ;
    char    *   buf;

    if (filename==NULL) return ;

    /* Get file size in bytes */
    if (stat(filename, &sta)!=0) {
        return ;
    }
    size = (int)sta.st_size ;
    /* Compute number of zeros to pad */
    remaining = size % FITS_BLOCK_SIZE ;
    if (remaining==0) return ;
    remaining = FITS_BLOCK_SIZE - remaining ;

    /* Open file, dump zeros, exit */
    if ((out=fopen(filename, "a"))==NULL)
        return ;
    buf = calloc(remaining, sizeof(char));
    fwrite(buf, 1, remaining, out);
    fclose(out);
    free(buf);
    return ;
}

/*----------------------------------------------------------------------------*/
/**
  @brief    Identify if a file is a FITS file.
  @param    filename name of the file to check
  @return   int 0, 1, or -1

  Returns 1 if the file name looks like a valid FITS file. Returns
  0 else. If the file does not exist, returns -1.
 */
/*----------------------------------------------------------------------------*/
int is_fits_file(char *filename)
{
    FILE  *   fp ;
    char  *   magic ;
    int       isfits ;

    if (filename==NULL) return -1 ;
    if ((fp = fopen(filename, "r"))==NULL) {
        qfits_error("cannot open file [%s]", filename) ;
        return -1 ;
    }

    magic = calloc(FITS_MAGIC_SZ+1, sizeof(char)) ;
    fread(magic, 1, FITS_MAGIC_SZ, fp) ;
    fclose(fp) ;
    magic[FITS_MAGIC_SZ] = (char)0 ;
    if (strstr(magic, FITS_MAGIC)!=NULL)
        isfits = 1 ;
    else
        isfits = 0 ;
    free(magic) ;
    return isfits ;
}

/* vim: set ts=4 et sw=4 tw=75 */
