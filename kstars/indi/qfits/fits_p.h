/*----------------------------------------------------------------------------*/
/**
  @file     fits_p.h
  @author   N. Devillard
  @date     Mar 2000
  @version  $Revision$
  @brief    FITS parser for a single card

  This module contains various routines to help parsing a single FITS
  card into its components: key, value, comment.
*/
/*----------------------------------------------------------------------------*/

/*
	$Id$
	$Author$
	$Date$
	$Revision$
*/

#ifndef FITSEP_H
#define FITSEP_H

#ifdef __cplusplus
extern "C" {
#endif

/* <dox> */
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
/* </dox> */

#ifdef __cplusplus
}
#endif

#endif
/* vim: set ts=4 et sw=4 tw=75 */
