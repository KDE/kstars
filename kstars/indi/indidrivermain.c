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

/* main() for one INDI driver process.
 * Drivers define IS*() functions we call to deliver INDI XML arriving on stdin.
 * Drivers call ID*() functions to send INDI XML commands to stdout.
 * Drivers call IE*() functions to build an event-driver program.
 * Drivers call IU*() functions to perform various common utility tasks.
 * Troubles are reported on stderr then we exit.
 *
 * This requires liblilxml.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "lilxml.h"
#include "base64.h"
#include "eventloop.h"
#include "indidevapi.h"
#include "indicom.h"

static void usage(void);
static void clientMsgCB(int fd, void *arg);
static int dispatch (XMLEle *root, char msg[]);
static int crackDN (XMLEle *root, char **dev, char **name, char msg[]);
const char *pstateStr(IPState s);
const char *sstateStr(ISState s);
const char *ruleStr(ISRule r);
const char *permStr(IPerm p);

static int nroCheck;			/* # of elements in roCheck */
static int verbose;			/* chatty */
char *me;				/* a.out name */
static LilXML *clixml;			/* XML parser context */

/* insure RO properties are never modified. RO Sanity Check */
typedef struct
{
  char propName[MAXINDINAME];
  IPerm perm;
} ROSC;

static ROSC *roCheck;

int
main (int ac, char *av[])
{
        setgid( getgid() );
        setuid( getuid() );
        if (geteuid() != getuid())
            exit(255);

	/* save handy pointer to our base name */
	for (me = av[0]; av[0][0]; av[0]++)
	    if (av[0][0] == '/')
		me = &av[0][1];

	/* crack args */
	while (--ac && (*++av)[0] == '-')
	    while (*++(*av))
		switch (*(*av)) {
		case 'v':	/* verbose */
		    verbose++;
		    break;
		default:
		    usage();
		}

	/* ac remaining args starting at av[0] */
	if (ac > 0)
	    usage();

	/* init */
	clixml =  newLilXML();
	addCallback (0, clientMsgCB, NULL);
	
	nroCheck = 0;
	roCheck = NULL;

	/* service client */
	eventLoop();

	/* eh?? */
	fprintf (stderr, "%s: inf loop ended\n", me);
	return (1);
}

/* functions we define that drivers may call */

/* tell client to create a text vector property */
void
IDDefText (const ITextVectorProperty *tvp, const char *fmt, ...)
{
	int i;
	ROSC *SC;

	printf ("<defTextVector\n");
	printf ("  device='%s'\n", tvp->device);
	printf ("  name='%s'\n", tvp->name);
	printf ("  label='%s'\n", tvp->label);
	printf ("  group='%s'\n", tvp->group);
	printf ("  state='%s'\n", pstateStr(tvp->s));
	printf ("  perm='%s'\n", permStr(tvp->p));
	printf ("  timeout='%g'\n", tvp->timeout);
	printf ("  timestamp='%s'\n", timestamp());
	if (fmt) {
	    va_list ap;
	    va_start (ap, fmt);
	    printf ("  message='");
	    vprintf (fmt, ap);
	    printf ("'\n");
	    va_end (ap);
	}
	printf (">\n");

	for (i = 0; i < tvp->ntp; i++) {
	    IText *tp = &tvp->tp[i];
	    printf ("  <defText\n");
	    printf ("    name='%s'\n", tp->name);
	    printf ("    label='%s'>\n", tp->label);
	    printf ("      %s\n", tp->text ? tp->text : "");
	    printf ("  </defText>\n");
	}

	printf ("</defTextVector>\n");
	
	/* Add this property to insure proper sanity check */
	roCheck = roCheck ? (ROSC *) realloc ( roCheck, sizeof(ROSC) * (nroCheck+1))
	                  : (ROSC *) malloc  ( sizeof(ROSC));
	SC      = &roCheck[nroCheck++];
	
	strcpy(SC->propName, tvp->name);
	SC->perm = tvp->p;
	
	fflush (stdout);
}

/* tell client to create a new numeric vector property */
void
IDDefNumber (const INumberVectorProperty *n, const char *fmt, ...)
{
	int i;
	ROSC *SC;

	printf ("<defNumberVector\n");
	printf ("  device='%s'\n", n->device);
	printf ("  name='%s'\n", n->name);
	printf ("  label='%s'\n", n->label);
	printf ("  group='%s'\n", n->group);
	printf ("  state='%s'\n", pstateStr(n->s));
	printf ("  perm='%s'\n", permStr(n->p));
	printf ("  timeout='%g'\n", n->timeout);
	printf ("  timestamp='%s'\n", timestamp());
	if (fmt) {
	    va_list ap;
	    va_start (ap, fmt);
	    printf ("  message='");
	    vprintf (fmt, ap);
	    printf ("'\n");
	    va_end (ap);
	}
	printf (">\n");

	for (i = 0; i < n->nnp; i++) {
	    INumber *np = &n->np[i];
	    printf ("  <defNumber\n");
	    printf ("    name='%s'\n", np->name);
	    printf ("    label='%s'\n", np->label);
	    printf ("    format='%s'\n", np->format);
	    printf ("    min='%g'\n", np->min);
	    printf ("    max='%g'\n", np->max);
	    printf ("    step='%g'>\n", np->step);
	    printf ("      %g\n", np->value);
	    printf ("  </defNumber>\n");
	}

	printf ("</defNumberVector>\n");
	
	/* Add this property to insure proper sanity check */
	roCheck = roCheck ? (ROSC *) realloc ( roCheck, sizeof(ROSC) * (nroCheck+1))
	                  : (ROSC *) malloc  ( sizeof(ROSC));
	SC      = &roCheck[nroCheck++];
	
	strcpy(SC->propName, n->name);
	SC->perm = n->p;
	
	fflush (stdout);
}

/* tell client to create a new switch vector property */
void
IDDefSwitch (const ISwitchVectorProperty *s, const char *fmt, ...)

{
	int i;
	ROSC *SC;

	printf ("<defSwitchVector\n");
	printf ("  device='%s'\n", s->device);
	printf ("  name='%s'\n", s->name);
	printf ("  label='%s'\n", s->label);
	printf ("  group='%s'\n", s->group);
	printf ("  state='%s'\n", pstateStr(s->s));
	printf ("  perm='%s'\n", permStr(s->p));
	printf ("  rule='%s'\n", ruleStr (s->r));
	printf ("  timeout='%g'\n", s->timeout);
	printf ("  timestamp='%s'\n", timestamp());
	if (fmt) {
	    va_list ap;
	    va_start (ap, fmt);
	    printf ("  message='");
	    vprintf (fmt, ap);
	    printf ("'\n");
	    va_end (ap);
	}
	printf (">\n");

	for (i = 0; i < s->nsp; i++) {
	    ISwitch *sp = &s->sp[i];
	    printf ("  <defSwitch\n");
	    printf ("    name='%s'\n", sp->name);
	    printf ("    label='%s'>\n", sp->label);
	    printf ("      %s\n", sstateStr(sp->s));
	    printf ("  </defSwitch>\n");
	}

	printf ("</defSwitchVector>\n");
	
	/* Add this property to insure proper sanity check */
	roCheck = roCheck ? (ROSC *) realloc ( roCheck, sizeof(ROSC) * (nroCheck+1))
	                  : (ROSC *) malloc  ( sizeof(ROSC));
	SC      = &roCheck[nroCheck++];
	
	strcpy(SC->propName, s->name);
	SC->perm = s->p;
	
	fflush (stdout);
}

/* tell client to create a new lights vector property */
void
IDDefLight (const ILightVectorProperty *lvp, const char *fmt, ...)
{
	int i;

	printf ("<defLightVector\n");
	printf ("  device='%s'\n", lvp->device);
	printf ("  name='%s'\n", lvp->name);
	printf ("  label='%s'\n", lvp->label);
	printf ("  group='%s'\n", lvp->group);
	printf ("  state='%s'\n", pstateStr(lvp->s));
	printf ("  timestamp='%s'\n", timestamp());
	if (fmt) {
	    va_list ap;
	    va_start (ap, fmt);
	    printf ("  message='");
	    vprintf (fmt, ap);
	    printf ("'\n");
	    va_end (ap);
	}
	printf (">\n");

	for (i = 0; i < lvp->nlp; i++) {
	    ILight *lp = &lvp->lp[i];
	    printf ("  <defLight\n");
	    printf ("    name='%s'\n", lp->name);
	    printf ("    label='%s'>\n", lp->label);
	    printf ("      %s\n", pstateStr(lp->s));
	    printf ("  </defLight>\n");
	}

	printf ("</defLightVector>\n");
	fflush (stdout);
}

/* tell client to create a new BLOB vector property */
void
IDDefBLOB (const IBLOBVectorProperty *b, const char *fmt, ...)
{
  int i;

	printf ("<defBLOBVector\n");
	printf ("  device='%s'\n", b->device);
	printf ("  name='%s'\n", b->name);
	printf ("  label='%s'\n", b->label);
	printf ("  group='%s'\n", b->group);
	printf ("  state='%s'\n", pstateStr(b->s));
	printf ("  perm='%s'\n", permStr(b->p));
	printf ("  timeout='%g'\n", b->timeout);
	printf ("  timestamp='%s'\n", timestamp());
	if (fmt) {
	    va_list ap;
	    va_start (ap, fmt);
	    printf ("  message='");
	    vprintf (fmt, ap);
	    printf ("'\n");
	    va_end (ap);
	}
	printf (">\n");

  for (i = 0; i < b->nbp; i++) {
    IBLOB *bp = &b->bp[i];
    printf ("  <defBLOB\n");
    printf ("    name='%s'\n", bp->name);
    printf ("    label='%s'\n", bp->label);
    printf ("  />\n");
  }

	printf ("</defBLOBVector>\n");
	fflush (stdout);
}

/* tell client to update an existing text vector property */
void
IDSetText (const ITextVectorProperty *tvp, const char *fmt, ...)
{
	int i;

	printf ("<setTextVector\n");
	printf ("  device='%s'\n", tvp->device);
	printf ("  name='%s'\n", tvp->name);
	printf ("  state='%s'\n", pstateStr(tvp->s));
	printf ("  timeout='%g'\n", tvp->timeout);
	printf ("  timestamp='%s'\n", timestamp());
	if (fmt) {
	    va_list ap;
	    va_start (ap, fmt);
	    printf ("  message='");
	    vprintf (fmt, ap);
	    printf ("'\n");
	    va_end (ap);
	}
	printf (">\n");

	for (i = 0; i < tvp->ntp; i++) {
	    IText *tp = &tvp->tp[i];
	    printf ("  <oneText name='%s'>\n", tp->name);
	    printf ("      %s\n", tp->text ? tp->text : "");
	    printf ("  </oneText>\n");
	}

	printf ("</setTextVector>\n");
	fflush (stdout);
}

/* tell client to update an existing numeric vector property */
void
IDSetNumber (const INumberVectorProperty *nvp, const char *fmt, ...)
{
	int i;

	printf ("<setNumberVector\n");
	printf ("  device='%s'\n", nvp->device);
	printf ("  name='%s'\n", nvp->name);
	printf ("  state='%s'\n", pstateStr(nvp->s));
	printf ("  timeout='%g'\n", nvp->timeout);
	printf ("  timestamp='%s'\n", timestamp());
	if (fmt) {
	    va_list ap;
	    va_start (ap, fmt);
	    printf ("  message='");
	    vprintf (fmt, ap);
	    printf ("'\n");
	    va_end (ap);
	}
	printf (">\n");

	for (i = 0; i < nvp->nnp; i++) {
	    INumber *np = &nvp->np[i];
	    printf ("  <oneNumber name='%s'>\n", np->name);
	    printf ("      %.10g\n", np->value);
	    printf ("  </oneNumber>\n");
	}

	printf ("</setNumberVector>\n");
	fflush (stdout);
}

/* tell client to update an existing switch vector property */
void
IDSetSwitch (const ISwitchVectorProperty *svp, const char *fmt, ...)
{
	int i;

	printf ("<setSwitchVector\n");
	printf ("  device='%s'\n", svp->device);
	printf ("  name='%s'\n", svp->name);
	printf ("  state='%s'\n", pstateStr(svp->s));
	printf ("  timeout='%g'\n", svp->timeout);
	printf ("  timestamp='%s'\n", timestamp());
	if (fmt) {
	    va_list ap;
	    va_start (ap, fmt);
	    printf ("  message='");
	    vprintf (fmt, ap);
	    printf ("'\n");
	    va_end (ap);
	}
	printf (">\n");

	for (i = 0; i < svp->nsp; i++) {
	    ISwitch *sp = &svp->sp[i];
	    printf ("  <oneSwitch name='%s'>\n", sp->name);
	    printf ("      %s\n", sstateStr(sp->s));
	    printf ("  </oneSwitch>\n");
	}

	printf ("</setSwitchVector>\n");
	fflush (stdout);
}

/* tell client to update an existing lights vector property */
void
IDSetLight (const ILightVectorProperty *lvp, const char *fmt, ...)
{
	int i;

	printf ("<setLightVector\n");
	printf ("  device='%s'\n", lvp->device);
	printf ("  name='%s'\n", lvp->name);
	printf ("  state='%s'\n", pstateStr(lvp->s));
	printf ("  timestamp='%s'\n", timestamp());
	if (fmt) {
	    va_list ap;
	    va_start (ap, fmt);
	    printf ("  message='");
	    vprintf (fmt, ap);
	    printf ("'\n");
	    va_end (ap);
	}
	printf (">\n");

	for (i = 0; i < lvp->nlp; i++) {
	    ILight *lp = &lvp->lp[i];
	    printf ("  <oneLight name='%s'>\n", lp->name);
	    printf ("      %s\n", pstateStr(lp->s));
	    printf ("  </oneLight>\n");
	}

	printf ("</setLightVector>\n");
	fflush (stdout);
}

/* tell client to update an existing BLOB vector property */
void
IDSetBLOB (const IBLOBVectorProperty *bvp, const char *fmt, ...)
{
	int i;

	printf ("<setBLOBVector\n");
	printf ("  device='%s'\n", bvp->device);
	printf ("  name='%s'\n", bvp->name);
	printf ("  state='%s'\n", pstateStr(bvp->s));
	printf ("  timeout='%g'\n", bvp->timeout);
	printf ("  timestamp='%s'\n", timestamp());
	if (fmt) {
	    va_list ap;
	    va_start (ap, fmt);
	    printf ("  message='");
	    vprintf (fmt, ap);
	    printf ("'\n");
	    va_end (ap);
	}
	printf (">\n");

	for (i = 0; i < bvp->nbp; i++) {
	    IBLOB *bp = &bvp->bp[i];
	    char *encblob;
	    int j, l;

	    printf ("  <oneBLOB\n");
	    printf ("    name='%s'\n", bp->name);
	    printf ("    size='%d'\n", bp->size);
	    printf ("    format='%s'>\n", bp->format);

	    encblob = malloc (4*bp->bloblen/3+4);
	    l = to64frombits(encblob, bp->blob, bp->bloblen);
	    for (j = 0; j < l; j += 72)
		printf ("%.72s\n", encblob+j);
	    free (encblob);

	    printf ("  </oneBLOB>\n");
	}

  printf ("</setBLOBVector>\n");
  fflush (stdout);
}

/* tell client to update min/max elements of an existing number vector property */
void IUUpdateMinMax(INumberVectorProperty *nvp)
{
  int i;

  printf ("<setNumberVector\n");
  printf ("  device='%s'\n", nvp->device);
  printf ("  name='%s'\n", nvp->name);
  printf ("  state='%s'\n", pstateStr(nvp->s));
  printf ("  timeout='%g'\n", nvp->timeout);
  printf ("  timestamp='%s'\n", timestamp());
  printf (">\n");

  for (i = 0; i < nvp->nnp; i++) {
    INumber *np = &nvp->np[i];
    printf ("  <oneNumber name='%s'\n", np->name);
    printf ("    min='%g'\n", np->min);
    printf ("    max='%g'\n", np->max);
    printf ("    step='%g'\n", np->step);
    printf(">\n");
    printf ("      %g\n", np->value);
    printf ("  </oneNumber>\n");
  }

  printf ("</setNumberVector>\n");
  fflush (stdout);
}

/* send client a message for a specific device or at large if !dev */
void
IDMessage (const char *dev, const char *fmt, ...)
{
	printf ("<message\n");
	if (dev)
	    printf (" device='%s'\n", dev);
	printf ("  timestamp='%s'\n", timestamp());
	if (fmt) {
	    va_list ap;
	    va_start (ap, fmt);
	    printf ("  message='");
	    vprintf (fmt, ap);
	    printf ("'\n");
	    va_end (ap);
	}
	printf ("/>\n");
	fflush (stdout);
}

/* tell Client to delete the property with given name on given device, or
 * entire device if !name
 */
void
IDDelete (const char *dev, const char *name, const char *fmt, ...)
{
	printf ("<delProperty\n  device='%s'\n", dev);
	if (name)
	    printf (" name='%s'\n", name);
	printf ("  timestamp='%s'\n", timestamp());
	if (fmt) {
	    va_list ap;
	    va_start (ap, fmt);
	    printf ("  message='");
	    vprintf (fmt, ap);
	    printf ("'\n");
	    va_end (ap);
	}
	printf ("/>\n");
	fflush (stdout);
}

/* log message locally.
 * this has nothing to do with XML or any Clients.
 */
void
IDLog (const char *fmt, ...)
{
	va_list ap;
	fprintf (stderr, "%s ", timestamp());
	va_start (ap, fmt);
	vfprintf (stderr, fmt, ap);
	va_end (ap);
}

/* "INDI" wrappers to the more generic eventloop facility. */

int
IEAddCallback (int readfiledes, IE_CBF *fp, void *p)
{
	return (addCallback (readfiledes, (CBF*)fp, p));
}

void
IERmCallback (int callbackid)
{
	rmCallback (callbackid);
}

int
IEAddTimer (int millisecs, IE_TCF *fp, void *p)
{
	return (addTimer (millisecs, (TCF*)fp, p));
}

void
IERmTimer (int timerid)
{
	rmTimer (timerid);
}

int
IEAddWorkProc (IE_WPF *fp, void *p)
{
	return (addWorkProc ((WPF*)fp, p));
}

void
IERmWorkProc (int workprocid)
{
	rmWorkProc (workprocid);
}

/* find a member of an IText vector, else NULL */
IText *
IUFindText  (const ITextVectorProperty *tvp, const char *name)
{
	int i;

	for (i = 0; i < tvp->ntp; i++)
	    if (strcmp (tvp->tp[i].name, name) == 0)
		return (&tvp->tp[i]);
	fprintf (stderr, "No IText '%s' in %s.%s\n",name,tvp->device,tvp->name);
	return (NULL);
}

/* find a member of an INumber vector, else NULL */
INumber *
IUFindNumber(const INumberVectorProperty *nvp, const char *name)
{
	int i;

	for (i = 0; i < nvp->nnp; i++)
	    if (strcmp (nvp->np[i].name, name) == 0)
		return (&nvp->np[i]);
	fprintf(stderr,"No INumber '%s' in %s.%s\n",name,nvp->device,nvp->name);
	return (NULL);
}

/* find a member of an ISwitch vector, else NULL */
ISwitch *
IUFindSwitch(const ISwitchVectorProperty *svp, const char *name)
{
	int i;

	for (i = 0; i < svp->nsp; i++)
	    if (strcmp (svp->sp[i].name, name) == 0)
		return (&svp->sp[i]);
	fprintf(stderr,"No ISwitch '%s' in %s.%s\n",name,svp->device,svp->name);
	return (NULL);
}

/* find an ON member of an ISwitch vector, else NULL.
 * N.B. user must make sense of result with ISRule in mind.
 */
ISwitch *
IUFindOnSwitch(const ISwitchVectorProperty *svp)
{
	int i;

	for (i = 0; i < svp->nsp; i++)
	    if (svp->sp[i].s == ISS_ON)
		return (&svp->sp[i]);
	fprintf(stderr, "No ISwitch On in %s.%s\n", svp->device, svp->name);
	return (NULL);
}

/* Set all switches to off */
void 
IUResetSwitches(const ISwitchVectorProperty *svp)
{
  int i;
  
  for (i = 0; i < svp->nsp; i++)
    svp->sp[i].s = ISS_OFF;
}

/* Update property switches in accord with states and names. */
int 
IUUpdateSwitches(ISwitchVectorProperty *svp, ISState *states, char *names[], int n)
{
 int i=0;
 
 ISwitch *sp;
 
 for (i = 0; i < n ; i++)
 {
   sp = IUFindSwitch(svp, names[i]);
	 
   if (!sp)
   {
              svp->s = IPS_IDLE;
	      IDSetSwitch(svp, "Error: %s is not a member of %s property.", names[i], svp->name);
	      return -1;
   }
	 
   sp->s = states[i]; 
 }
 
 return 0;

}

/* Update property numbers in accord with values and names */
int IUUpdateNumbers(INumberVectorProperty *nvp, double values[], char *names[], int n)
{
  int i=0;
  
  INumber *np;
  
  for (i = 0; i < n; i++)
  {
    np = IUFindNumber(nvp, names[i]);
    if (!np)
    {
    	nvp->s = IPS_IDLE;
	IDSetNumber(nvp, "Error: %s is not a member of %s property.", names[i], nvp->name);
	return -1;
    }
    
    if (values[i] < np->min || values[i] > np->max)
    {
       nvp->s = IPS_IDLE;
       IDSetNumber(nvp, "Error: Invalid range. Valid range is from %g to %g", np->min, np->max);
       return -1;
    }
      
  }

  /* First loop checks for error, second loop set all values atomically*/
  for (i=0; i < n; i++)
  {
    np = IUFindNumber(nvp, names[i]);
    np->value = values[i];
  }

  return 0;

}

/* save malloced copy of newtext in tp->text, reusing if not first time */
void
IUSaveText (IText *tp, const char *newtext)
{
	/* seed for realloc */
	if (tp->text == NULL)
	    tp->text = malloc (1);

	/* copy in fresh string */
	tp->text = strcpy (realloc (tp->text, strlen(newtext)+1), newtext);
}

void fillSwitch(ISwitch *sp, const char *name, const char * label, ISState s)
{
  strcpy(sp->name, name);
  strcpy(sp->label, label);
  sp->s = s;
  sp->svp = NULL;
  sp->aux = NULL;
}

void fillNumber(INumber *np, const char *name, const char * label, const char *format, double min, double max, double step, double value)
{

  strcpy(np->name, name);
  strcpy(np->label, label);
  strcpy(np->format, format);
  
  np->min	= min;
  np->max	= max;
  np->step	= step;
  np->value	= value;
  np->nvp	= NULL;
  np->aux0	= NULL;
  np->aux1	= NULL;
}

void fillText(IText *tp, const char *name, const char * label, const char *initialText)
{

  strcpy(tp->name, name);
  strcpy(tp->label, label);
  tp->text = NULL;
  tp->tvp  = NULL;
  tp->aux0 = NULL;
  tp->aux1 = NULL;

  IUSaveText(tp, initialText);

}

void fillSwitchVector(ISwitchVectorProperty *svp, ISwitch *sp, int nsp, const char * dev, const char *name, const char *label, const char *group, IPerm p, ISRule r, double timeout, IPState s)
{
  strcpy(svp->device, dev);
  strcpy(svp->name, name);
  strcpy(svp->label, label);
  strcpy(svp->group, group);
  strcpy(svp->timestamp, "");
  
  svp->p	= p;
  svp->r	= r;
  svp->timeout	= timeout;
  svp->s	= s;
  svp->sp	= sp;
  svp->nsp	= nsp;
}
 
void fillNumberVector(INumberVectorProperty *nvp, INumber *np, int nnp, const char * dev, const char *name, const char *label, const char* group, IPerm p, double timeout, IPState s)
{
  
  strcpy(nvp->device, dev);
  strcpy(nvp->name, name);
  strcpy(nvp->label, label);
  strcpy(nvp->group, group);
  strcpy(nvp->timestamp, "");
  
  nvp->p	= p;
  nvp->timeout	= timeout;
  nvp->s	= s;
  nvp->np	= np;
  nvp->nnp	= nnp;
  
}

void fillTextVector(ITextVectorProperty *tvp, IText *tp, int ntp, const char * dev, const char *name, const char *label, const char* group, IPerm p, double timeout, IPState s)
{
  strcpy(tvp->device, dev);
  strcpy(tvp->name, name);
  strcpy(tvp->label, label);
  strcpy(tvp->group, group);
  strcpy(tvp->timestamp, "");
  
  tvp->p	= p;
  tvp->timeout	= timeout;
  tvp->s	= s;
  tvp->tp	= tp;
  tvp->ntp	= ntp;

}

/* print usage message and exit (1) */
static void
usage(void)
{
	fprintf (stderr, "Usage: %s [options]\n", me);
	fprintf (stderr, "Purpose: INDI Device driver framework.\n");
	fprintf (stderr, "Options:\n");
	fprintf (stderr, " -v    : more verbose to stderr\n");

	exit (1);
}

/* callback when INDI client message arrives on stdin.
 * collect and dispatch when see outter element closure.
 * exit if OS trouble or see incompatable INDI version.
 * arg is not used.
 */
static void
clientMsgCB (int fd, void *arg)
{
	char buf[1024], msg[1024], *bp;
	int nr;
	arg=arg;

	/* one read */
	nr = read (fd, buf, sizeof(buf));
	if (nr < 0) {
	    fprintf (stderr, "%s: %s\n", me, strerror(errno));
	    exit(1);
	}
	if (nr == 0) {
	    fprintf (stderr, "%s: EOF\n", me);
	    exit(1);
	}

	/* crack and dispatch when complete */
	for (bp = buf; nr-- > 0; bp++) {
	    XMLEle *root = readXMLEle (clixml, *bp, msg);
	    if (root) {
		if (dispatch (root, msg) < 0)
		    fprintf (stderr, "%s dispatch error: %s\n", me, msg);
		delXMLEle (root);
	    } else if (msg[0])
		fprintf (stderr, "%s XML error: %s\n", me, msg);
	}
}

/* crack the given INDI XML element and call driver's IS* entry points as they
 *   are recognized.
 * return 0 if ok else -1 with reason in msg[].
 * N.B. exit if getProperties does not proclaim a compatible version.
 */
static int
dispatch (XMLEle *root, char msg[])
{
	XMLEle *epx;
	int n;
        int i=0;

	if (verbose)
	    prXMLEle (stderr, root, 0);

	/* check tag in surmised decreasing order of likelyhood */

	if (!strcmp (tagXMLEle(root), "newNumberVector")) {
	    static double *doubles;
	    static char **names;
	    static int maxn;
	    char *dev, *name;

	    /* pull out device and name */
	    if (crackDN (root, &dev, &name, msg) < 0)
		return (-1);

	    /* seed for reallocs */
	    if (!doubles) {
		doubles = (double *) malloc (1);
		names = (char **) malloc (1);
	    }

	    /* pull out each name/value pair */
	    for (n = 0, epx = nextXMLEle(root,1); epx; epx = nextXMLEle(root,0)) {
		if (strcmp (tagXMLEle(epx), "oneNumber") == 0) {
		    XMLAtt *na = findXMLAtt (epx, "name");
		    if (na) {
			if (n >= maxn) {
			    /* grow for this and another */
			    int newsz = (maxn=n+1)*sizeof(double);
			    doubles = (double *) realloc(doubles,newsz);
			    newsz = maxn*sizeof(char *);
			    names = (char **) realloc (names, newsz);
			}
			if (f_scansexa (pcdataXMLEle(epx), &doubles[n]) < 0)
			    IDMessage (dev,"%s: Bad format %s", name,
							    pcdataXMLEle(epx));
			else
			    names[n++] = valuXMLAtt(na);
		    }
		}
	    }
	    
	    /* insure property is not RO */
	    for (i=0; i < nroCheck; i++)
	    {
	      if (!strcmp(roCheck[i].propName, name))
	      {
	       if (roCheck[i].perm == IP_RO)
	         return -1;
	      }
	    }

	    /* invoke driver if something to do, but not an error if not */
	    if (n > 0)
		ISNewNumber (dev, name, doubles, names, n);
	    else
		IDMessage(dev,"%s: newNumberVector with no valid members",name);
	    return (0);
	}

	if (!strcmp (tagXMLEle(root), "newSwitchVector")) {
	    static ISState *states;
	    static char **names;
	    static int maxn;
	    char *dev, *name;

	    /* pull out device and name */
	    if (crackDN (root, &dev, &name, msg) < 0)
		return (-1);

	    /* seed for reallocs */
	    if (!states) {
		states = (ISState *) malloc (1);
		names = (char **) malloc (1);
	    }

	    /* pull out each name/state pair */
	    for (n = 0, epx = nextXMLEle(root,1); epx; epx = nextXMLEle(root,0)) {
		if (strcmp (tagXMLEle(epx), "oneSwitch") == 0) {
		    XMLAtt *na = findXMLAtt (epx, "name");
		    if (na) {
			if (n >= maxn) {
			    int newsz = (maxn=n+1)*sizeof(ISState);
			    states = (ISState *) realloc(states, newsz);
			    newsz = maxn*sizeof(char *);
			    names = (char **) realloc (names, newsz);
			}
			if (strcmp (pcdataXMLEle(epx),"On") == 0) {
			    states[n] = ISS_ON;
			    names[n] = valuXMLAtt(na);
			    n++;
			} else if (strcmp (pcdataXMLEle(epx),"Off") == 0) {
			    states[n] = ISS_OFF;
			    names[n] = valuXMLAtt(na);
			    n++;
			} else 
			    IDMessage (dev, "%s: must be On or Off: %s", name,
							    pcdataXMLEle(epx));
		    }
		}
	    }

	    /* insure property is not RO */
	    for (i=0; i < nroCheck; i++)
	    {
	      if (!strcmp(roCheck[i].propName, name))
	      {
	       if (roCheck[i].perm == IP_RO)
	         return -1;
	      }
	    }
	    
	    /* invoke driver if something to do, but not an error if not */
	    if (n > 0)
		ISNewSwitch (dev, name, states, names, n);
	    else
		IDMessage(dev,"%s: newSwitchVector with no valid members",name);
	    return (0);
	}

	if (!strcmp (tagXMLEle(root), "newTextVector")) {
	    static char **texts;
	    static char **names;
	    static int maxn;
	    char *dev, *name;

	    /* pull out device and name */
	    if (crackDN (root, &dev, &name, msg) < 0)
		return (-1);

	    /* seed for reallocs */
	    if (!texts) {
		texts = (char **) malloc (1);
		names = (char **) malloc (1);
	    }

	    /* pull out each name/text pair */
	    for (n = 0, epx = nextXMLEle(root,1); epx; epx = nextXMLEle(root,0)) {
		if (strcmp (tagXMLEle(epx), "oneText") == 0) {
		    XMLAtt *na = findXMLAtt (epx, "name");
		    if (na) {
			if (n >= maxn) {
			    int newsz = (maxn=n+1)*sizeof(char *);
			    texts = (char **) realloc (texts, newsz);
			    names = (char **) realloc (names, newsz);
			}
			texts[n] = pcdataXMLEle(epx);
			names[n] = valuXMLAtt(na);
			n++;
		    }
		}
	    }
	    
	    /* insure property is not RO */
	    for (i=0; i < nroCheck; i++)
	    {
	      if (!strcmp(roCheck[i].propName, name))
	      {
	       if (roCheck[i].perm == IP_RO)
	         return -1;
	      }
	    }

	    /* invoke driver if something to do, but not an error if not */
	    if (n > 0)
		ISNewText (dev, name, texts, names, n);
	    else
		IDMessage (dev, "%s: set with no valid members", name);
	    return (0);
	}

	if (!strcmp (tagXMLEle(root), "newBLOBVector")) {
	    static char **blobs;
	    static char **names;
	    static char **formats;
	    static int *sizes;
	    static int maxn;
	    char *dev, *name;

	    /* pull out device and name */
	    if (crackDN (root, &dev, &name, msg) < 0)
		return (-1);

	    /* seed for reallocs */
	    if (!blobs) {
		blobs = (char **) malloc (1);
		names = (char **) malloc (1);
		formats = (char **) malloc (1);
		sizes = (int *) malloc (1);
	    }

	    /* pull out each name/BLOB pair */
	    for (n = 0, epx = nextXMLEle(root,1); epx; epx = nextXMLEle(root,0)) {
		if (strcmp (tagXMLEle(epx), "oneBLOB") == 0) {
		    XMLAtt *na = findXMLAtt (epx, "name");
		    XMLAtt *fa = findXMLAtt (epx, "format");
		    XMLAtt *sa = findXMLAtt (epx, "size");
		    if (na && fa && sa) {
			if (n >= maxn) {
			    int newsz = (maxn=n+1)*sizeof(char *);
			    blobs = (char **) realloc (blobs, newsz);
			    names = (char **) realloc (names, newsz);
			    formats = (char **) realloc(formats,newsz);
			    newsz = maxn*sizeof(int);
			    sizes = (int *) realloc(sizes,newsz);
			}
			blobs[n] = pcdataXMLEle(epx);
			names[n] = valuXMLAtt(na);
			formats[n] = valuXMLAtt(fa);
			sizes[n] = atoi(valuXMLAtt(sa));
			n++;
		    }
		}
	    }

	    /* invoke driver if something to do, but not an error if not */
	    if (n > 0)
		ISNewBLOB (dev, name, sizes, blobs, formats, names, n);
	    else
		IDMessage (dev, "%s: newBLOBVector with no valid members",name);
	    return (0);
	}

	if (!strcmp (tagXMLEle(root), "getProperties")) {
	    XMLAtt *ap;
	    double v;

	    /* check version */
	    ap = findXMLAtt (root, "version");
	    if (!ap) {
		fprintf (stderr, "%s: getProperties missing version\n", me);
		exit(1);
	    }
	    v = atof (valuXMLAtt(ap));
	    if (v > INDIV) {
		fprintf (stderr, "%s: client version %g > %g\n", me, v, INDIV);
		exit(1);
	    }

	    /* ok */
	    ap = findXMLAtt (root, "device");
	    ISGetProperties (ap ? valuXMLAtt(ap) : NULL);
	    return (0);
	}

	sprintf (msg, "Unknown command: %s", tagXMLEle(root));
	return(1);
}

/* pull out device and name attributes from root.
 * return 0 if ok else -1 with reason in msg[].
 */
static int
crackDN (XMLEle *root, char **dev, char **name, char msg[])
{
	XMLAtt *ap;

	ap = findXMLAtt (root, "device");
	if (!ap) {
	    sprintf (msg, "%s requires 'device' attribute", tagXMLEle(root));
	    return (-1);
	}
	*dev = valuXMLAtt(ap);

	ap = findXMLAtt (root, "name");
	if (!ap) {
	    sprintf (msg, "%s requires 'name' attribute", tagXMLEle(root));
	    return (-1);
	}
	*name = valuXMLAtt(ap);

	return (0);
}

/* return static string corresponding to the given property or light state */
const char *
pstateStr (IPState s)
{
	switch (s) {
	case IPS_IDLE:  return ("Idle");
	case IPS_OK:    return ("Ok");
	case IPS_BUSY:  return ("Busy");
	case IPS_ALERT: return ("Alert");
	default:
	    fprintf (stderr, "Impossible IPState %d\n", s);
	    exit(1);
	}
}

/* return static string corresponding to the given switch state */
const char *
sstateStr (ISState s)
{
	switch (s) {
	case ISS_ON:  return ("On");
	case ISS_OFF: return ("Off");
	default:
	    fprintf (stderr, "Impossible ISState %d\n", s);
	    exit(1);
	}
}

/* return static string corresponding to the given Rule */
const char *
ruleStr (ISRule r)
{
	switch (r) {
	case ISR_1OFMANY: return ("OneOfMany");
	case ISR_ATMOST1: return ("AtMostOne");
	case ISR_NOFMANY: return ("AnyOfMany");
	default:
	    fprintf (stderr, "Impossible ISRule %d\n", r);
	    exit(1);
	}
}

/* return static string corresponding to the given IPerm */
const char *
permStr (IPerm p)
{
	switch (p) {
	case IP_RO: return ("ro");
	case IP_WO: return ("wo");
	case IP_RW: return ("rw");
	default:
	    fprintf (stderr, "Impossible IPerm %d\n", p);
	    exit(1);
	}
}

