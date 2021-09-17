/*
** Author: Eric Veach, July 1994.
**
*/

#ifndef __memalloc_simple_h_
#define __memalloc_simple_h_

#include <stdlib.h>

#define memRealloc realloc
#define memFree    free

#define memInit __gl_memInit
/*extern void		__gl_memInit( size_t );*/
extern int __gl_memInit(size_t);

#ifndef MEMORY_DEBUG
#define memAlloc malloc
#else
#define memAlloc __gl_memAlloc
extern void *__gl_memAlloc(size_t);
#endif

#endif
