/*----------------------------------------------------------------------------*/
/**
  @file		fits_p.c
  @author	N. Devillard
  @date		Mar 2000
  @version	$Revision$
  @brief	FITS parser for a single card

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

/*-----------------------------------------------------------------------------
   								Includes
 -----------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "simple.h"

/*-----------------------------------------------------------------------------
   								Defines
 -----------------------------------------------------------------------------*/

/* Define the following to get zillions of debug messages */
/* #define DEBUG_FITSHEADER */

/*-----------------------------------------------------------------------------
  							Function codes
 -----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/**
  @brief	Find the keyword in a key card (80 chars)	
  @param	line allocated 80-char line from a FITS header
  @return	statically allocated char *

  Find out the part of a FITS line corresponding to the keyword.
  Returns NULL in case of error. The returned pointer is statically
  allocated in this function, so do not modify or try to free it.
 */
/*----------------------------------------------------------------------------*/
char * qfits_getkey(char * line)
{
	static char 	key[81];
	int				i ;

	if (line==NULL) {
#ifdef DEBUG_FITSHEADER
		printf("qfits_getkey: NULL input line\n");
#endif
		return NULL ;
	}

	/* Special case: blank keyword */
	if (!strncmp(line, "        ", 8)) {
		strcpy(key, "        ");
		return key ;
	}
	/* Sort out special cases: HISTORY, COMMENT, END do not have = in line */
	if (!strncmp(line, "HISTORY ", 8)) {
		strcpy(key, "HISTORY");
		return key ;
	}
	if (!strncmp(line, "COMMENT ", 8)) {
		strcpy(key, "COMMENT");
		return key ;
	}
	if (!strncmp(line, "END ", 4)) {
		strcpy(key, "END");
		return key ;
	}

	memset(key, 0, 81);
	/* General case: look for the first equal sign */
	i=0 ;
	while (line[i]!='=' && i<80) i++ ;
	if (i>=80) {
#ifdef DEBUG_FITSHEADER
		printf("qfits_getkey: cannot find equal sign\n");
#endif
		return NULL ;
	}
	i-- ;
	/* Equal sign found, now backtrack on blanks */
	while (line[i]==' ' && i>=0) i-- ;
	if (i<=0) {
#ifdef DEBUG_FITSHEADER
		printf("qfits_getkey: error backtracking on blanks\n");
#endif
		return NULL ;
	}
	i++ ;

	/* Copy relevant characters into output buffer */
	strncpy(key, line, i) ;
	/* Null-terminate the string */
	key[i+1] = (char)0;
	return key ;
}

/*----------------------------------------------------------------------------*/
/**
  @brief	Find the value in a key card (80 chars)	
  @param	line allocated 80-char line from a FITS header
  @return	statically allocated char *

  Find out the part of a FITS line corresponding to the value.
  Returns NULL in case of error, or if no value can be found. The
  returned pointer is statically allocated in this function, so do not
  modify or try to free it.
 */
/*----------------------------------------------------------------------------*/
char * qfits_getvalue(char * line)
{
    static char value[81] ;
    int     i ;
    int     from, to ;
    int     inq ;

	if (line==NULL) {
#ifdef DEBUG_FITSHEADER
		printf("qfits_getvalue: NULL input line\n");
#endif
		return NULL ;
	}

	/* Special cases */

	/* END has no associated value */
	if (!strncmp(line, "END ", 4)) {
		return NULL ;
	}
	/*
	 * HISTORY has for value everything else on the line, stripping
	 * blanks before and after. Blank HISTORY is also accepted.
	 */
	memset(value, 0, 81);

	if (!strncmp(line, "HISTORY ", 8) || !strncmp(line, "        ", 8)) {
		i=7 ;
		/* Strip blanks from the left side */
		while (line[i]==' ' && i<80) i++ ;
		if (i>=80) return NULL ; /* Blank HISTORY */
		from=i ;

		/* Strip blanks from the right side */
		to=79 ;
		while (line[to]==' ') to-- ;
		/* Copy relevant characters into output buffer */
		strncpy(value, line+from, to-from+1);
		/* Null-terminate the string */
		value[to-from+1] = (char)0;
		return value ;
	} else if (!strncmp(line, "COMMENT ", 8)) {
	    /* COMMENT is like HISTORY */
        /* Strip blanks from the left side */
        i=7 ;
		while (line[i]==' ' && i<80) i++ ;
		if (i>=80) return NULL ;
		from=i ;

		/* Strip blanks from the right side */
		to=79 ;
		while (line[to]==' ') to-- ;

		if (to<from) {
#ifdef DEBUG_FITSHEADER
			printf("qfits_getvalue: inconsistent value search in COMMENT\n");
#endif
			return NULL ;
		}
		/* Copy relevant characters into output buffer */
		strncpy(value, line+from, to-from+1);
		/* Null-terminate the string */
		value[to-from+1] = (char)0;
		return value ;
	}
	/* General case - Get past the keyword */
    i=0 ;
    while (line[i]!='=' && i<80) i++ ;
    if (i>80) {
#ifdef DEBUG_FITSHEADER
		printf("qfits_getvalue: no equal sign found on line\n");
#endif
		return NULL ;
	}
    i++ ;
    while (line[i]==' ' && i<80) i++ ;
    if (i>80) {
#ifdef DEBUG_FITSHEADER
		printf("qfits_getvalue: no value past the equal sign\n");
#endif
		return NULL ;
	}
    from=i;

	/* Now value section: Look for the first slash '/' outside a string */
    inq = 0 ;
    while (i<80) {
        if (line[i]=='\'')
			inq=!inq ;
        if (line[i]=='/')
            if (!inq)
                break ;
        i++;
    }
    i-- ;

	/* Backtrack on blanks */
    while (line[i]==' ' && i>=0) i-- ;
    if (i<0) {
#ifdef DEBUG_FITSHEADER
		printf("qfits_getvalue: error backtracking on blanks\n");
#endif
		return NULL ;
	}
    to=i ;

	if (to<from) {
#ifdef DEBUG_FITSHEADER
		printf("qfits_getvalue: from>to?\n");
		printf("line=[%s]\n", line);
#endif
		return NULL ;
	}
	/* Copy relevant characters into output buffer */
    strncpy(value, line+from, to-from+1);
	/* Null-terminate the string */
    value[to-from+1] = (char)0;
    return value ;
}

/*----------------------------------------------------------------------------*/
/**
  @brief	Find the comment in a key card (80 chars)	
  @param	line allocated 80-char line from a FITS header
  @return	statically allocated char *

  Find out the part of a FITS line corresponding to the comment.
  Returns NULL in case of error, or if no comment can be found. The
  returned pointer is statically allocated in this function, so do not
  modify or try to free it.
 */
/*----------------------------------------------------------------------------*/
char * qfits_getcomment(char * line)
{
	static char comment[81];
	int	i ;
	int	from, to ;
	int	inq ;

	if (line==NULL) {
#ifdef DEBUG_FITSHEADER
		printf("qfits_getcomment: null line in input\n");
#endif
		return NULL ;
	}

	/* Special cases: END, HISTORY, COMMENT and blank have no comment */
	if (!strncmp(line, "END ", 4)) return NULL ;
	if (!strncmp(line, "HISTORY ", 8)) return NULL ;
	if (!strncmp(line, "COMMENT ", 8)) return NULL ;
	if (!strncmp(line, "        ", 8)) return NULL ;

	memset(comment, 0, 81);
	/* Get past the keyword */
	i=0 ;
	while (line[i]!='=' && i<80) i++ ;
	if (i>=80) {
#ifdef DEBUG_FITSHEADER
		printf("qfits_getcomment: no equal sign on line\n");
#endif
		return NULL ;
	}
	i++ ;
	
	/* Get past the value until the slash */
	inq = 0 ;
	while (i<80) {
		if (line[i]=='\'')
			inq = !inq ;
		if (line[i]=='/')
			if (!inq)
				break ;
		i++ ;
	}
	if (i>=80) {
#ifdef DEBUG_FITSHEADER
		printf("qfits_getcomment: no slash found on line\n");
#endif
		return NULL ;
	}
	i++ ;
	/* Get past the first blanks */
	while (line[i]==' ') i++ ;
	from=i ;

	/* Now backtrack from the end of the line to the first non-blank char */
	to=79 ;
	while (line[to]==' ') to-- ;

	if (to<from) {
#ifdef DEBUG_FITSHEADER
		printf("qfits_getcomment: from>to?\n");
#endif
		return NULL ;
	}
	/* Copy relevant characters into output buffer */
	strncpy(comment, line+from, to-from+1);
	/* Null-terminate the string */
	comment[to-from+1] = (char)0;
	return comment ;
}

/* vim: set ts=4 et sw=4 tw=75 */
