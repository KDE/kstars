/*----------------------------------------------------------------------------*/
/**
  @file		expkey.c
  @author	N. Devillard
  @date		Feb 2001
  @version	$Revisions$
  @brief	Expand keyword from shortFITS to HIERARCH notation

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

/*-----------------------------------------------------------------------------
   								Includes
 -----------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "static_sz.h"

/*-----------------------------------------------------------------------------
  							Function codes
 -----------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------*/
/**
  @brief    Uppercase a string
  @param    s   string
  @return   string
 */
/*----------------------------------------------------------------------------*/
static char * expkey_strupc(char * s)
{
    static char l[ASCIILINESZ+1];
    int i ;

    if (s==NULL) return NULL ;
    memset(l, 0, ASCIILINESZ+1);
    i=0 ;
    while (s[i] && i<ASCIILINESZ) {
        l[i] = (char)toupper((int)s[i]);
        i++ ;
    }
    l[ASCIILINESZ]=(char)0;
    return l ;
}

/*----------------------------------------------------------------------------*/
/**
  @brief	Expand a keyword from shortFITS to HIERARCH notation.
  @param	keyword		Keyword to expand.
  @return	1 pointer to statically allocated string.

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
char * qfits_expand_keyword(char * keyword)
{
	static char expanded[81];
	char		ws[81];
	char	*	token ;

	/* Bulletproof entries */
	if (keyword==NULL) return NULL ;
	/* If regular keyword, copy the uppercased input and return */
	if (strstr(keyword, ".")==NULL) {
		strcpy(expanded, expkey_strupc(keyword));
		return expanded ;
	}
	/* Regular shortFITS keyword */
	sprintf(expanded, "HIERARCH ESO");
	strcpy(ws, expkey_strupc(keyword));
	token = strtok(ws, ".");
	while (token!=NULL) {
		strcat(expanded, " ");
		strcat(expanded, token);
		token = strtok(NULL, ".");
	}
	return expanded ;
}
/* vim: set ts=4 et sw=4 tw=75 */
