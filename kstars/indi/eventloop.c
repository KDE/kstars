#if 0
    INDI
    Copyright (C) 2003 Elwood C. Downey

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

#endif

/* suite of functions to implement an event driven program.
 *
 * callbacks may be registered that are triggered when a file descriptor
 *   will not block when read;
 *
 * timers may be registered that will run no sooner than a specified delay from
 *   the moment they were registered;
 *
 * work procedures may be registered that are called when there is nothing
 *   else to do;
 *
 #define MAIN_TEST for a stand-alone test program.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>

#include "eventloop.h"

/* info about one registered callback. */
typedef struct {
    int in_use;				/* flag to make this record is active */
    int fd;				/* fd descriptor to watch for read */
    void *ud;				/* user's data handle */
    CBF *fp;				/* callback function */
    int cid;				/* unique id for this callback */
} CB;

/* info about one registered timer function */
typedef struct {
    int tgo;				/* trigger time, ms from epoch */
    void *ud;				/* user's data handle */
    TCF *fp;				/* timer function */
    int tid;				/* unique id for this timer */
} TF;

/* info about one registered work procedure. */
typedef struct {
    int in_use;				/* flag to make this record is active */
    void *ud;				/* user's data handle */
    WPF *fp;				/* work proc function function */
    int wid;				/* unique id for this work proc */
} WP;


static CB *cback;			/* malloced list of callbacks */
static int ncback;			/* n entries in cback[] */
static int cid;				/* source of callback ids */

static TF *timef;			/* malloced list of timer functions */
static int ntimef;			/* n entries in ntimef[] */
static struct timeval epoch;		/* arbitrary t0 */
static int tid;				/* source of timer ids */
#define	EPDT(tp) 			/* ms from epoch to timeval *tp */  \
	(((tp)->tv_sec-epoch.tv_sec)*1000 + ((tp)->tv_usec-epoch.tv_usec)/1000)

static WP *wproc;			/* malloced list of work procedures */
static int nwproc;			/* n entries in wproc[] */
static int nwpinuse;			/* n entries in wproc[] marked in-use */
static int wid;				/* source of worproc ids */

static void runWorkProcs (void);
static void callCallbacks(fd_set *rfdp, int nready);
static void popTimers();
static void oneLoop(void);

/* inf loop to dispatch callbacks, work procs and timers as necessary.
 * never returns.
 */
void
eventLoop()
{
	/* init epoch to now */
	gettimeofday (&epoch, NULL);

	/* run loop forever */
	while (1)
	    oneLoop();
}

/* register a new callback, fp, to be called with ud as arg when fd is ready.
 * return a unique callback id for use with rmCallback().
 */
int
addCallback (int fd, CBF *fp, void *ud)
{
	CB *cp;

	for (cp = cback; cp < &cback[ncback]; cp++)
	    if (!cp->in_use)
		break;

	if (cp == &cback[ncback]) {
	    cback = cback ? (CB *) realloc (cback, (ncback+1)*sizeof(CB))
	    		  : (CB *) malloc (sizeof(CB));
	    cp = &cback[ncback++];
	}

	cp->in_use = 1;
	cp->fp = fp;
	cp->ud = ud;
	cp->fd = fd;
	return (cp->cid = ++cid);
}

/* remove the callback with the given id, as returned from addCallback().
 * silently ignore if id not found.
 */
void
rmCallback (int callbackid)
{
	CB *cp;

	for (cp = cback; cp < &cback[ncback]; cp++) {
	    if (cp->in_use && cp->cid == callbackid) {
		cp->in_use = 0;
		break;
	    }
	}
}

/* register a new timer function, fp, to be called with ud as arg after ms
 * milliseconds. add to list in order of decreasing time from epoch, ie,
 * last entry runs soonest. return id for use with rmTimer().
 */
int
addTimer (int ms, TCF *fp, void *ud)
{
	struct timeval t;
	TF *tp;

	gettimeofday (&t, NULL);

	timef = timef ? (TF *) realloc (timef, (ntimef+1)*sizeof(TF))
		      : (TF *) malloc (sizeof(TF));
	tp = &timef[ntimef++];

	tp->ud = ud;
	tp->fp = fp;
	tp->tgo = EPDT(&t) + ms;

	for ( ; tp > timef && tp[0].tgo > tp[-1].tgo; tp--) {
	    TF tmptf = tp[-1];
	    tp[-1] = tp[0];
	    tp[0] = tmptf;
	}

	return (tp->tid = ++tid);
}

/* remove the timer with the given id, as returned from addTimer().
 * silently ignore if id not found.
 */
void
rmTimer (int timerID)
{
	TF *tp;

	/* find it */
	for (tp = timef; tp < &timef[ntimef]; tp++)
	    if (tp->tid == timerID)
		break;
	if (tp == &timef[ntimef])
	    return;

	/* bubble it out */
	for (++tp; tp < &timef[ntimef]; tp++)
	    tp[-1] = tp[0];

	/* shrink list */
	timef = (TF *) realloc (timef, (--ntimef)*sizeof(CB));
}

/* add a new work procedure, fp, to be called with ud when nothing else to do.
 * return unique id for use with rmWorkProc().
 */
int
addWorkProc (WPF *fp, void *ud)
{
	WP *wp;

	for (wp = wproc; wp < &wproc[nwproc]; wp++)
	    if (!wp->in_use)
		break;

	if (wp == &wproc[nwproc]) {
	    wproc = wproc ? (WP *) realloc (wproc, (nwproc+1)*sizeof(WP))
	    		  : (WP *) malloc (sizeof(WP));
	    wp = &wproc[nwproc++];
	}

	wp->in_use = 1;
	wp->fp = fp;
	wp->ud = ud;
	nwpinuse++;
	return (wp->wid = ++wid);
}


/* remove the work proc with the given id, as returned from addWorkProc().
 * silently ignore if id not found.
 */
void
rmWorkProc (int workID)
{
	WP *wp;

	for (wp = wproc; wp < &wproc[nwproc]; wp++) {
	    if (wp->in_use && wp->wid == workID) {
		if (wp == &wproc[nwproc-1] && wp > wproc)
		    wproc = (WP *) realloc (wproc, (--nwproc)*sizeof(WP));
		else
		    wp->in_use = 0;
		nwpinuse--;
		break;
	    }
	}
}

/* run all registered work procedures */
static void
runWorkProcs ()
{
	WP *wp;

	for (wp = wproc; wp < &wproc[nwproc]; wp++)
	    if (wp->in_use)
		(*wp->fp) (wp->ud);
}

/* run all registered callbacks whose fd is listed in rfdp */
static void
callCallbacks(fd_set *rfdp, int nready)
{
	CB *cp;

	for (cp = cback; nready > 0 && cp < &cback[ncback]; cp++) {
	    if (cp->in_use && FD_ISSET (cp->fd, rfdp)) {
		(*cp->fp) (cp->fd, cp->ud);
		nready--;
	    }
	}
}

/* run all timers that are ready to pop. timef[] is sorted such in decreasing
 * order of time from epoch to run, ie, last entry runs soonest.
 */
static void
popTimers()
{
	struct timeval now;
	int tgonow;
	TF *tp;

	gettimeofday (&now, NULL);
	tgonow = EPDT (&now);
	for (tp = &timef[ntimef-1]; tp >= timef && tp->tgo <= tgonow; tp--) {
	    ntimef--;
	    (*tp->fp) (tp->ud);
	    printf ("\a\n");
	}
}

/* check fd's from each active callback.
 * if any ready, call their callbacks else call each registered work procedure.
 */
static void
oneLoop()
{
	struct timeval tv, *tvp;
	fd_set rfd;
	CB *cp;
	int maxfd, ns;

	/* build list of file descriptors to check */
	FD_ZERO (&rfd);
	maxfd = -1;
	for (cp = cback; cp < &cback[ncback]; cp++) {
	    if (cp->in_use) {
		FD_SET (cp->fd, &rfd);
		if (cp->fd > maxfd)
		    maxfd = cp->fd;
	    }
	}

	/* if there are work procs
	 *   set delay = 0
	 * else if there is at least one timer func
	 *   set delay = time until soonest timer func expires
	 * else
	 *   set delay = forever
	 */

	if (nwpinuse > 0) {
	    tvp = &tv;
	    tvp->tv_sec = tvp->tv_usec = 0;
	} else if (ntimef > 0) {
	    struct timeval now;
	    int late;
	    gettimeofday (&now, NULL);
	    late = timef[ntimef-1].tgo - EPDT (&now);
	    if (late < 0)
		late = 0;
	    tvp = &tv;
	    tvp->tv_sec = late/1000;
	    tvp->tv_usec = 1000*(late%1000);
	} else
	    tvp = NULL;

	/* check file descriptors, dispatch callbacks or workprocs as per info*/
	ns = select (maxfd+1, &rfd, NULL, NULL, tvp);
	if (ns < 0) {
	    perror ("select");
	    exit(1);
	}
	
	/* dispatch */
	if (ns == 0)
	    runWorkProcs();
	else
	    callCallbacks(&rfd, ns);
	if (ntimef > 0)
	    popTimers();
}

#if defined(MAIN_TEST)
/* make a small stand-alone test program.
 */

#include <unistd.h>
#include <sys/time.h>

int counter;
int mycid;
int mywid;
int mytid;

char user_a = 'A';
char user_b = 'B';

void
wp (void *ud)
{
	struct timeval tv;
	char a = *(char *)ud;

	gettimeofday (&tv, NULL);
	printf ("workproc: %c @ %ld.%03ld counter %d\n", a, (long)tv.tv_sec,
						(long)tv.tv_usec/1000, counter);
}

void
to (void *ud)
{
	printf ("timeout %d\n", (int)ud);
}

void
stdinCB (int fd, void *ud)
{
	char b = *(char *)ud;
	char c;

	if (read (fd, &c, 1) != 1)
	    exit(1);

	switch (c) {
	case '+': counter++; break;
	case '-': counter--; break;

	case 'W': mywid = addWorkProc (wp, &user_b); break;
	case 'w': rmWorkProc (mywid); break;

	case 'c': rmCallback (mycid); break;

	case 't': rmTimer (mytid); break;
	case '1': mytid = addTimer (1000, to, (void *)1); break;
	case '2': mytid = addTimer (2000, to, (void *)2); break;
	case '3': mytid = addTimer (3000, to, (void *)3); break;
	case '4': mytid = addTimer (4000, to, (void *)4); break;
	case '5': mytid = addTimer (5000, to, (void *)5); break;
	default: return;	/* silently absorb other chars like \n */
	}

	printf ("callback: %c counter is now %d\n", b, counter);
}

int
main (int ac, char *av[])
{
	cid = addCallback (0, stdinCB, &user_a);
	eventLoop();
	exit(0);
}

#endif

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile$ $Date$ $Revision$ $Name:  $"};
