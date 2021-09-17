/*
** Author: Eric Veach, July 1994.
**
*/

#ifndef __priorityq_sort_h_
#define __priorityq_sort_h_

#include "priorityq-heap.h"

#undef PQkey
#undef PQhandle
#undef PriorityQ
#undef pqNewPriorityQ
#undef pqDeletePriorityQ
#undef pqInit
#undef pqInsert
#undef pqMinimum
#undef pqExtractMin
#undef pqDelete
#undef pqIsEmpty

/* Use #define's so that another heap implementation can use this one */

#define PQkey     PQSortKey
#define PQhandle  PQSortHandle
#define PriorityQ PriorityQSort

#define pqNewPriorityQ(leq)   __gl_pqSortNewPriorityQ(leq)
#define pqDeletePriorityQ(pq) __gl_pqSortDeletePriorityQ(pq)

/* The basic operations are insertion of a new key (pqInsert),
 * and examination/extraction of a key whose value is minimum
 * (pqMinimum/pqExtractMin).  Deletion is also allowed (pqDelete);
 * for this purpose pqInsert returns a "handle" which is supplied
 * as the argument.
 *
 * An initial heap may be created efficiently by calling pqInsert
 * repeatedly, then calling pqInit.  In any case pqInit must be called
 * before any operations other than pqInsert are used.
 *
 * If the heap is empty, pqMinimum/pqExtractMin will return a NULL key.
 * This may also be tested with pqIsEmpty.
 */
#define pqInit(pq)           __gl_pqSortInit(pq)
#define pqInsert(pq, key)    __gl_pqSortInsert(pq, key)
#define pqMinimum(pq)        __gl_pqSortMinimum(pq)
#define pqExtractMin(pq)     __gl_pqSortExtractMin(pq)
#define pqDelete(pq, handle) __gl_pqSortDelete(pq, handle)
#define pqIsEmpty(pq)        __gl_pqSortIsEmpty(pq)

/* Since we support deletion the data structure is a little more
 * complicated than an ordinary heap.  "nodes" is the heap itself;
 * active nodes are stored in the range 1..pq->size.  When the
 * heap exceeds its allocated size (pq->max), its size doubles.
 * The children of node i are nodes 2i and 2i+1.
 *
 * Each node stores an index into an array "handles".  Each handle
 * stores a key, plus a pointer back to the node which currently
 * represents that key (ie. nodes[handles[i].node].handle == i).
 */

typedef PQHeapKey PQkey;
typedef PQHeapHandle PQhandle;
typedef struct PriorityQ PriorityQ;

struct PriorityQ
{
    PriorityQHeap *heap;
    PQkey *keys;
    PQkey **order;
    PQhandle size, max;
    int initialized;
    int (*leq)(PQkey key1, PQkey key2);
};

PriorityQ *pqNewPriorityQ(int (*leq)(PQkey key1, PQkey key2));
void pqDeletePriorityQ(PriorityQ *pq);

int pqInit(PriorityQ *pq);
PQhandle pqInsert(PriorityQ *pq, PQkey key);
PQkey pqExtractMin(PriorityQ *pq);
void pqDelete(PriorityQ *pq, PQhandle handle);

PQkey pqMinimum(PriorityQ *pq);
int pqIsEmpty(PriorityQ *pq);

#endif
