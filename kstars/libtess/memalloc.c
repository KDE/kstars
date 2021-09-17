/*
** Author: Eric Veach, July 1994.
**
*/

#include "memalloc.h"
#include "string.h"

int __gl_memInit(size_t maxFast)
{
#ifndef NO_MALLOPT
/*  mallopt( M_MXFAST, maxFast );*/
#ifdef MEMORY_DEBUG
    mallopt(M_DEBUG, 1);
#endif
#endif
    return 1;
}

#ifdef MEMORY_DEBUG
void *__gl_memAlloc(size_t n)
{
    return memset(malloc(n), 0xa5, n);
}
#endif
