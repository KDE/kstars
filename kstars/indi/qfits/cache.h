/*----------------------------------------------------------------------------*/
/**
  @file     cache.h
  @author   N. Devillard
  @date     Mar 2001
  @version  $Revision$
  @brief    FITS caching capabilities

  This modules implements a cache for FITS access routines.
  The first time a FITS file is seen by the library, all corresponding
  pointers are cached here. This speeds up multiple accesses to large
  files by magnitudes.
*/
/*----------------------------------------------------------------------------*/

/*
	$Id$
	$Author$
	$Date$
	$Revision$
*/

#ifndef CACHE_H
#define CACHE_H

/*-----------------------------------------------------------------------------
   								Includes
 -----------------------------------------------------------------------------*/

#include <stdio.h>

/*-----------------------------------------------------------------------------
   								Defines
 -----------------------------------------------------------------------------*/

/** Query the number of extensions */
#define QFITS_QUERY_N_EXT		(1<<30)
/** Query the offset to header start */
#define QFITS_QUERY_HDR_START	(1<<29)
/** Query the offset to data start */
#define QFITS_QUERY_DAT_START	(1<<28)
/** Query header size in bytes */
#define QFITS_QUERY_HDR_SIZE	(1<<27)
/** Query data size in bytes */
#define QFITS_QUERY_DAT_SIZE	(1<<26)

/* <dox> */
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
/* </dox> */

/*----------------------------------------------------------------------------*/
/**
  @brief    Query a FITS file offset from the cache.
  @param    filename    Name of the file to examine.
  @param    what        What should be queried (see below).
  @return   an integer offset, or -1 if an error occurred.

  This function queries the cache for FITS offset information. If the
  requested file name has never been seen before, it is completely parsed
  to extract all offset informations, which are then stored in the cache.
  The next query will get the informations from the cache, avoiding
  a complete re-parsing of the file. This is especially useful for large
  FITS files with lots of extensions, because querying the extensions
  is an expensive operation.

  This operation has side-effects: the cache is an automatically
  allocated structure in memory, that can only grow. Every request
  on a new FITS file will make it grow. The structure is pretty
  light-weight in memory, but nonetheless this is an issue for daemon-type
  programs which must run over long periods. The solution is to clean
  the cache using qfits_cache_purge() at regular intervals. This is left
  to the user of this library.

  To request information about a FITS file, you must pass an integer
  built from the following symbols:

  - @c QFITS_QUERY_N_EXT
  - @c QFITS_QUERY_HDR_START
  - @c QFITS_QUERY_DAT_START
  - @c QFITS_QUERY_HDR_SIZE
  - @c QFITS_QUERY_DAT_SIZE

  Querying the number of extensions present in a file is done
  simply with:

  @code
  next = qfits_query(filename, QFITS_QUERY_N_EXT);
  @endcode

  Querying the offset to the i-th extension header is done with:

  @code
  off = qfits_query(filename, QFITS_QUERY_HDR_START | i);
  @endcode

  i.e. you must OR (|) the extension number with the
  @c QFITS_QUERY_HDR_START symbol. Requesting offsets to extension data is
  done in the same way:

  @code
  off = qfits_query(filename, QFITS_QUERY_DAT_START | i);
  @endcode

  Notice that extension 0 is the main header and main data part
  of the FITS file.
 */
/*----------------------------------------------------------------------------*/
int qfits_query(char * filename, int what);

#endif
/* vim: set ts=4 et sw=4 tw=75 */
