/*----------------------------------------------------------------------------*/
/**
  @file     xmemory.h
  @author   Nicolas Devillard
  @date     Oct 2000
  @version  $Revision$
  @brief    POSIX-compatible extended memory handling.

  xmemory is a small and efficient module offering memory extension
  capabitilies to ANSI C programs running on POSIX-compliant systems. It
  offers several useful features such as memory leak detection, protection for
  free on NULL or unallocated pointers, and virtually unlimited memory space.
  xmemory requires the @c mmap() system call to be implemented in the local C
  library to function. This module has been tested on a number of current Unix
  flavours and is reported to work fine.
  The current limitation is the limited number of pointers it can handle at
  the same time.
  See the documentation attached to this module for more information.
*/
/*----------------------------------------------------------------------------*/

/*
	$Id$
	$Author$
	$Date$
	$Revision$
*/

#ifndef XMEMORY_H
#define XMEMORY_H

/*-----------------------------------------------------------------------------
   								Includes
 -----------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*-----------------------------------------------------------------------------
   								Defines
 -----------------------------------------------------------------------------*/

/* To know if the current module has been linked against xmemory.c or not */
#define _XMEMORY_	1

/*-----------------------------------------------------------------------------
   								Macros
 -----------------------------------------------------------------------------*/

/* Protect strdup redefinition on systems which #define it */
#ifdef strdup
#undef strdup
#endif

#define malloc(s)       xmemory_malloc(s,       __FILE__,__LINE__)
#define calloc(n,s)     xmemory_calloc(n,s,     __FILE__,__LINE__)
#define realloc(p,s)    xmemory_realloc(p,s,    __FILE__,__LINE__)
#define free(p)         xmemory_free(p,         __FILE__,__LINE__)
#define strdup(s)       xmemory_strdup(s,       __FILE__,__LINE__)
#define falloc(f,o,s)   xmemory_falloc(f,o,s,   __FILE__,__LINE__)

/* Trick to have xmemory status display the file name and line */
#define xmemory_status() xmemory_status_(__FILE__,__LINE__)
#define xmemory_diagnostics() xmemory_status_(__FILE__,__LINE__)

/*-----------------------------------------------------------------------------
   							Function prototypes
 -----------------------------------------------------------------------------*/

void * 	xmemory_malloc(size_t, char *, int) ;
void * 	xmemory_calloc(size_t, size_t, char *, int) ;
void * 	xmemory_realloc(void *, size_t, char *, int) ;
void   	xmemory_free(void *, char *, int) ;
char * 	xmemory_strdup(char *, char *, int) ;
char *	xmemory_falloc(char *, size_t, size_t *, char *, int) ;

void xmemory_on(void) ;
void xmemory_off(void) ; 
void xmemory_status_(char * filename, int lineno) ;
void xmemory_settmpdir(char * dirname) ;
char * xmemory_gettmpdir(void);

#endif
/* vim: set ts=4 et sw=4 tw=75 */
