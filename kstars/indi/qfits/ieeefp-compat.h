/*----------------------------------------------------------------------------*/
/**
   @file    ieeefp-compat.h
   @author  N. Devillard
   @date    Feb 2002
   @version	$Revision$
   @brief   This module implements the isnan() and isinf() macros.
  
   The isnan() and isinf() macros are unfortunately not yet part of
   the standard C math library everywhere. They can usually be found
   in different places, if they are offered at all, and require the
   application to link against the math library. To avoid portability
   problems and linking against -lm, this module implements a fast
   and portable way of finding out whether a floating-point value
   (float or double) is a NaN or an Inf.

   Instead of calling isnan() and isinf(), the programmer including
   this file should call qfits_isnan() and qfits_isinf().
*/
/*----------------------------------------------------------------------------*/

/*
	$Id$
	$Author$
	$Date$
	$Revision$
*/

#ifndef IEEEFP_COMPAT_H
#define IEEEFP_COMPAT_H

/*-----------------------------------------------------------------------------
   								Macros
 -----------------------------------------------------------------------------*/

/* <dox> */
/*----------------------------------------------------------------------------*/
/**
  @brief	Test a floating-point variable for NaN value.
  @param    n   Number to test (float or double)
  @return   1 if n is NaN, 0 else.

  This macro is needed to support both float and double variables
  as input parameter. It checks on the size of the input variable
  to branch to the float or double version.

  Portability is an issue for this function which is present on
  most Unixes but not all, under various libraries (C lib on BSD,
  Math lib on Linux, sunmath on Solaris, ...). Integrating the
  code for this function makes qfits independent from any math
  library.
 */
/*----------------------------------------------------------------------------*/
#define qfits_isnan(n) ((sizeof(n)==sizeof(float)) ? _qfits_isnanf(n) : \
                        (sizeof(n)==sizeof(double)) ? _qfits_isnand(n) : -1)

/*----------------------------------------------------------------------------*/
/**
  @brief	Test a floating-point variable for Inf value.
  @param    n   Number to test (float or double)
  @return   1 if n is Inf or -Inf, 0 else.

  This macro is needed to support both float and double variables
  as input parameter. It checks on the size of the input variable
  to branch to the float or double version.

  Portability is an issue for this function which is missing on most
  Unixes. Most of the time, another function called finite() is
  offered to perform the opposite task, but it is not consistent
  among platforms and found in various libraries. Integrating the
  code for this function makes qfits independent from any math
  library.
 */
/*----------------------------------------------------------------------------*/
#define qfits_isinf(n) ((sizeof(n)==sizeof(float)) ? _qfits_isinff(n) : \
                        (sizeof(n)==sizeof(double)) ? _qfits_isinfd(n) : -1)

/*-----------------------------------------------------------------------------
   							Function prototypes
 -----------------------------------------------------------------------------*/
/**
 * Test a float variable for NaN value.
 * Do not call directly, call qfits_isnan().
 */
int _qfits_isnanf(float f);
/**
 * Test a float variable for Inf value.
 * Do not call directly, call qfits_isinf().
 */
int _qfits_isinff(float f);
/**
 * Test a double variable for NaN value.
 * Do not call directly, call qfits_isnan().
 */
int _qfits_isnand(double d);
/**
 * Test a double variable for Inf value.
 * Do not call directly, call qfits_isinf().
 */
int _qfits_isinfd(double d);

/* </dox> */
#endif
/* vim: set ts=4 et sw=4 tw=75 */
