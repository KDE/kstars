/*----------------------------------------------------------------------------*/
/**
   @file    pafs.h
   @author  N. Devillard
   @date    Feb 1999
   @version $Revision$
   @brief   PAF format I/O.

   This module contains various routines related to PAF file I/O.
*/
/*----------------------------------------------------------------------------*/

/*
	$Id$
	$Author$
	$Date$
	$Revision$
*/

#ifndef PAFS_H
#define PAFS_H

#ifdef __cplusplus
extern "C" {
#endif

/*-----------------------------------------------------------------------------
   								Includes
 ---------ii------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "static_sz.h"

/* <dox> */
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

/* </dox> */
#ifdef __cplusplus
}
#endif

#endif
