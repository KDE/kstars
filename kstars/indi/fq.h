/* a fifo queue that never fills.
 * licensed under GNU Lesser Public License version 2.1 or later.
 * Copyright (C) 2005 Elwood C. Downey ecdowney@clearskyinstitute.com
 */

typedef struct _FQ FQ;

extern FQ *newFQ(int grow);
extern void delFQ (FQ *q);
extern void pushFQ (FQ *q, void *e);
extern void *popFQ (FQ *q);
extern void *peekFQ (FQ *q);
extern int nFQ (FQ *q);
extern void setMemFuncsFQ (void *(*newmalloc)(size_t size),
   void *(*newrealloc)(void *ptr, size_t size),
   void (*newfree)(void *ptr));

