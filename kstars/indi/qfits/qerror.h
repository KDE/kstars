/*----------------------------------------------------------------------------*/
/**
   @file	qerror.h
   @author	N. Devillard
   @date	Nov 2001
   @version	$Revision$
   @brief	qfits error handling
*/
/*----------------------------------------------------------------------------*/

/*
	$Id$
	$Author$
	$Date$
	$Revision$
*/

#ifndef QERROR_H
#define QERROR_H

/*-----------------------------------------------------------------------------
   								Includes
 -----------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdarg.h>

/*-----------------------------------------------------------------------------
   							Function prototypes
 -----------------------------------------------------------------------------*/

/* <dox> */
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
/* </dox> */

/* Public warning/error functions */
void qfits_warning(char *fmt, ...);
void qfits_error(char *fmt, ...);

#endif
/* vim: set ts=4 et sw=4 tw=75 */
