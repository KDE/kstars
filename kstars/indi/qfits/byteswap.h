/*----------------------------------------------------------------------------*/
/**
   @file    byteswap.h
   @author  N. Devillard
   @date    Sep 1999
   @version $Revision$
   @brief   Low-level byte-swapping routines

   This module offers access to byte-swapping routines.
   Generic routines are offered that should work everywhere.
   Assembler is also included for x86 architectures, and dedicated
   assembler calls for processors > 386.
*/
/*----------------------------------------------------------------------------*/

/*
	$Id$
	$Author$
	$Date$
	$Revision$
*/

#ifndef BYTESWAP_H
#define BYTESWAP_H

/*-----------------------------------------------------------------------------
                                Includes
 -----------------------------------------------------------------------------*/

#include <stdlib.h>

/*-----------------------------------------------------------------------------
						Function ANSI C prototypes
 -----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/**
  @brief    Swap a 16-bit number
  @param    w A 16-bit (short) number to byte-swap.
  @return   The swapped version of w, w is untouched.

  This function swaps a 16-bit number, returned the swapped value without
  modifying the passed argument. Assembler included for x86 architectures.
 */
/*----------------------------------------------------------------------------*/
unsigned short swap_bytes_16(unsigned short w);

/*----------------------------------------------------------------------------*/
/**
  @brief    Swap a 32-bit number
  @param    dw A 32-bit (long) number to byte-swap.
  @return   The swapped version of dw, dw is untouched.

  This function swaps a 32-bit number, returned the swapped value without
  modifying the passed argument. Assembler included for x86 architectures
  and optimized for processors above 386.
 */
/*----------------------------------------------------------------------------*/
unsigned int swap_bytes_32(unsigned int dw);

/*----------------------------------------------------------------------------*/
/**
  @brief    Swaps bytes in a variable of given size
  @param    p pointer to void (generic pointer)
  @param    s size of the element to swap, pointed to by p
  @return   void

  This byte-swapper is portable and works for any even variable size.
  It is not truly the most efficient ever, but does its job fine
  everywhere this file compiles.
 */
/*----------------------------------------------------------------------------*/
void swap_bytes(void * p, int s);

/*----------------------------------------------------------------------------*/
/**
  @brief    Find out if the local machine is big or little endian
  @return   int 1 if local machine needs byteswapping (INTEL), 0 else.

  This function determines at run-time the endian-ness of the local
  machine. An INTEL-like processor needs byte-swapping, a
  MOTOROLA-like one does not.
 */
/*----------------------------------------------------------------------------*/
int need_byteswapping(void);

#endif
/* vim: set ts=4 et sw=4 tw=75 */
