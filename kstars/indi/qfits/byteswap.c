/*----------------------------------------------------------------------------*/
/**
   @file	byteswap.c
   @author	N. Devillard
   @date	Sep 1999
   @version	$Revision$
   @brief	Low-level byte-swapping routines

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

/*-----------------------------------------------------------------------------
   								Includes
 -----------------------------------------------------------------------------*/

#include "config.h"

/*-----------------------------------------------------------------------------
   								Defines
 -----------------------------------------------------------------------------*/

/** Potential tracing feature for gcc > 2.95 */
#if (__GNUC__>2) || ((__GNUC__ == 2) && (__GNUC_MINOR__ > 95))
#define __NOTRACE__ __attribute__((__no_instrument_function__))
#else
#define __NOTRACE__
#endif

/*-----------------------------------------------------------------------------
  							Function codes
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
unsigned short __NOTRACE__ swap_bytes_16(unsigned short w)
{
#ifdef CPU_X86
    __asm("xchgb %b0,%h0" :
          "=q" (w) :
          "0" (w));
    return w ;
#else
    return (((w) & 0x00ff) << 8 | ((w) & 0xff00) >> 8);
#endif
}

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
unsigned int __NOTRACE__ swap_bytes_32(unsigned int dw)
{
#ifdef CPU_X86
#if CPU_X86 > 386
 __asm("bswap   %0":
       "=r" (dw)   :
#else
 __asm("xchgb   %b0,%h0\n"
      " rorl    $16,%0\n"
      " xchgb   %b0,%h0":
      "=q" (dw)      :
#endif
      "0" (dw));
  return dw ;
#else
    return ((((dw) & 0xff000000) >> 24) | (((dw) & 0x00ff0000) >>  8) |
            (((dw) & 0x0000ff00) <<  8) | (((dw) & 0x000000ff) << 24));
#endif
}

/*----------------------------------------------------------------------------*/
/**
  @fn		swap_bytes
  @brief	Swaps bytes in a variable of given size
  @param	p pointer to void (generic pointer)
  @param	s size of the element to swap, pointed to by p
  @return	void

  This byte-swapper is portable and works for any even variable size.
  It is not truly the most efficient ever, but does its job fine
  everywhere this file compiles.
 */
/*----------------------------------------------------------------------------*/
void __NOTRACE__ swap_bytes(void * p, int s)
{
    unsigned char tmp, *a, *b ;

    a = (unsigned char*)p ;
    b = a + s ;

    while (a<b) {
        tmp = *a ;
        *a++ = *--b ;
        *b = tmp ;
    }
}

/*----------------------------------------------------------------------------*/
/**
  @brief	Find out if the local machine is big or little endian
  @return	int 1 if local machine needs byteswapping (INTEL), 0 else.

  This function determines at run-time the endian-ness of the local
  machine. An INTEL-like processor needs byte-swapping, a
  MOTOROLA-like one does not.
 */
/*----------------------------------------------------------------------------*/
int need_byteswapping(void)
{
    short     ps = 0xFF ;
    return ((*((char*)(&ps))) ? 1 : 0 ) ;
}

/* vim: set ts=4 et sw=4 tw=75 */
