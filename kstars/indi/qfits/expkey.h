/*----------------------------------------------------------------------------*/
/**
  @file     expkey.h
  @author   N. Devillard
  @date     Feb 2001
  @version  $Revisions$
  @brief    Expand keyword from shortFITS to HIERARCH notation

  This module offers a function that is reused in a number of different
  places.
*/
/*----------------------------------------------------------------------------*/

/*
	$Id$
	$Author$
	$Date$
	$Revision$
*/

#ifndef EXPKEY_H
#define EXPKEY_H

/*-----------------------------------------------------------------------------
  							Function prototypes
 -----------------------------------------------------------------------------*/

/* <dox> */
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
/* </dox> */

#endif
/* vim: set ts=4 et sw=4 tw=75 */
