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
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

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
#include "eventloop.h"
#include "indiapi.h"
#include "indicom.h"
/*#include "astro.h"*/

static void usage(void);
static void clientMsgCB(int fd, void *arg);
static int dispatch (XMLEle *root, char msg[]);
static int crackDN (XMLEle *root, char **dev, char **name, char msg[]);
static char *pstateStr(IPState s);
static char *sstateStr(ISState s);
static char *ruleStr(ISRule r);
static char *permStr(IPerm p);
static char *timestamp (void);

static int verbose;			/* chatty */
char *me;				/* a.out name */
static LilXML *clixml;			/* XML parser context */

int
main (int ac, char *av[])
{
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

	/* service client */
	eventLoop();

	/* eh?? */
	fprintf (stderr, "%s: inf loop ended\n", me);
	return (1);
}

/* functions we define that drivers may call */

/* tell client to create a text vector property */
void
IDDefText (const ITextVectorProperty *t)
{
	int i;

	printf ("<defTextVector\n");
	printf ("  device='%s'\n", t->device);
	printf ("  name='%s'\n", t->name);
	printf ("  label='%s'\n", t->label);
	printf ("  grouptag='%s'\n", t->grouptag ? t->grouptag : "");
	printf ("  perm='%s'\n", permStr(t->p));
	printf ("  timeout='%g'\n", t->timeout);
	printf ("  state='%s'>\n", pstateStr(t->s));

	for (i = 0; i < t->nt; i++) {
	    IText *tp = &t->t[i];
	    printf ("  <defText\n");
	    printf ("    name='%s'\n", tp->name);
	    printf ("    label='%s'>\n", tp->label);
	    printf ("      %s\n", tp->text ? tp->text : "");
	    printf ("  </defText>\n");
	}

	printf ("</defTextVector>\n");
	fflush (stdout);
}

/* tell client to create a new numeric vector property */
void
IDDefNumber (const INumberVectorProperty *n)
{
	int i;

	printf ("<defNumberVector\n");
	printf ("  device='%s'\n", n->device);
	printf ("  name='%s'\n", n->name);
	printf ("  label='%s'\n", n->label);
	printf ("  grouptag='%s'\n", n->grouptag ? n->grouptag : "");
	printf ("  perm='%s'\n", permStr(n->p));
	printf ("  timeout='%g'\n", n->timeout);
	printf ("  state='%s'>\n", pstateStr(n->s));

	for (i = 0; i < n->nn; i++) {
	    INumber *np = &n->n[i];
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
	fflush (stdout);
}

/* tell client to create a new switch vector property */
void
IDDefSwitch (const ISwitchVectorProperty *s)
{
	int i;

	printf ("<defSwitchVector\n");
	printf ("  device='%s'\n", s->device);
	printf ("  name='%s'\n", s->name);
	printf ("  label='%s'\n", s->label);
	printf ("  grouptag='%s'\n", s->grouptag ? s->grouptag : "");
	printf ("  perm='%s'\n", permStr(s->p));
	printf ("  rule='%s'\n", ruleStr (s->r));
	printf ("  timeout='%g'\n", s->timeout);
	printf ("  state='%s'>\n", pstateStr(s->s));

	for (i = 0; i < s->nsw; i++) {
	    ISwitch *sp = &s->sw[i];
	    printf ("  <defSwitch\n");
	    printf ("    name='%s'\n", sp->name);
	    printf ("    label='%s'>\n", sp->label);
	    printf ("      %s\n", sstateStr(sp->s));
	    printf ("  </defSwitch>\n");
	}

	printf ("</defSwitchVector>\n");
	fflush (stdout);
}

/* tell client to create a new lights vector property */
void
IDDefLight (const ILightVectorProperty *l)
{
	int i;

	printf ("<defLightVector\n");
	printf ("  device='%s'\n", l->device);
	printf ("  name='%s'\n", l->name);
	printf ("  label='%s'\n", l->label);
	printf ("  grouptag='%s'\n", l->grouptag ? l->grouptag : "");
	printf ("  state='%s'>\n", pstateStr(l->s));

	for (i = 0; i < l->nl; i++) {
	    ILight *lp = &l->l[i];
	    printf ("  <defLight\n");
	    printf ("    name='%s'\n", lp->name);
	    printf ("    label='%s'>\n", lp->label);
	    printf ("      %s\n", pstateStr(lp->s));
	    printf ("  </defLight>\n");
	}

	printf ("</defLightVector>\n");
	fflush (stdout);
}

/* tell client to update an existing text vector property */
void
IDSetText (const ITextVectorProperty *t, const char *msg, ...)
{
	int i;

	printf ("<setTextVector\n");
	printf ("  device='%s'\n", t->device);
	printf ("  name='%s'\n", t->name);
	printf ("  timeout='%g'\n", t->timeout);
	printf ("  state='%s'>\n", pstateStr(t->s));

	for (i = 0; i < t->nt; i++) {
	    IText *tp = &t->t[i];
	    printf ("  <setText\n");
	    printf ("    name='%s'>\n", tp->name);
	    printf ("      %s\n", tp->text ? tp->text : "");
	    printf ("  </setText>\n");
	}

	if (msg) {
	    va_list ap;
	    printf ("  <msg time='%s'>", timestamp());
	    va_start (ap, msg);
	    vprintf (msg, ap);
	    va_end (ap);
	    printf ("  </msg>\n");
	}

	printf ("</setTextVector>\n");
	fflush (stdout);
}

/* tell client to update an existing numeric vector property */
void
IDSetNumber (const INumberVectorProperty *n, const char *msg, ...)
{
	int i;

	printf ("<setNumberVector\n");
	printf ("  device='%s'\n", n->device);
	printf ("  name='%s'\n", n->name);
	printf ("  timeout='%g'\n", n->timeout);
	printf ("  state='%s'>\n", pstateStr(n->s));

	for (i = 0; i < n->nn; i++) {
	    INumber *np = &n->n[i];
	    printf ("  <setNumber\n");
	    printf ("    name='%s'>\n", np->name);
	    printf ("      %g\n", np->value);
	    printf ("  </setNumber>\n");
	}

	if (msg) {
	    va_list ap;
	    printf ("  <msg time='%s'>", timestamp());
	    va_start (ap, msg);
	    vprintf (msg, ap);
	    va_end (ap);
	    printf ("  </msg>\n");
	}

	printf ("</setNumberVector>\n");
	fflush (stdout);
}

/* tell client to update an existing switch vector property */
void
IDSetSwitch (const ISwitchVectorProperty *s, const char *msg, ...)
{
	int i;

	printf ("<setSwitchVector\n");
	printf ("  device='%s'\n", s->device);
	printf ("  name='%s'\n", s->name);
	printf ("  timeout='%g'\n", s->timeout);
	printf ("  state='%s'>\n", pstateStr(s->s));

	for (i = 0; i < s->nsw; i++) {
	    ISwitch *sp = &s->sw[i];
	    printf ("  <setSwitch\n");
	    printf ("    name='%s'>\n", sp->name);
	    printf ("      %s\n", sstateStr(sp->s));
	    printf ("  </setSwitch>\n");
	}

	if (msg) {
	    va_list ap;
	    printf ("  <msg time='%s'>", timestamp());
	    va_start (ap, msg);
	    vprintf (msg, ap);
	    va_end (ap);
	    printf ("  </msg>\n");
	}

	printf ("</setSwitchVector>\n");
	fflush (stdout);
}

/* tell client to update an existing lights vector property */
void
IDSetLight (const ILightVectorProperty *l, const char *msg, ...)
{
	int i;

	printf ("<setLightVector\n");
	printf ("  device='%s'\n", l->device);
	printf ("  name='%s'\n", l->name);
	printf ("  state='%s'>\n", pstateStr(l->s));

	for (i = 0; i < l->nl; i++) {
	    ILight *lp = &l->l[i];
	    printf ("  <setLight\n");
	    printf ("    name='%s'\n", lp->name);
	    printf ("      %s\n", pstateStr(lp->s));
	    printf ("  </setLight>\n");
	}

	if (msg) {
	    va_list ap;
	    printf ("  <msg time='%s'>", timestamp());
	    va_start (ap, msg);
	    vprintf (msg, ap);
	    va_end (ap);
	    printf ("  </msg>\n");
	}

	printf ("</setLightVector>\n");
	fflush (stdout);
}

/* send client a message for a specific device or at large if !dev */
void
IDMessage (const char *dev, const char *msg, ...)
{
	printf ("<message");
	if (dev)
	    printf (" device='%s'", dev);
	printf (">\n");

	if (msg) {
	    va_list ap;
	    printf ("  <msg time='%s'>", timestamp());
	    va_start (ap, msg);
	    vprintf (msg, ap);
	    va_end (ap);
	    printf ("  </msg>\n");
	}

	printf ("</message>");
	fflush (stdout);
}

/* tell Client to delete the property with given name on given device, or
 * entire device if !name
 */
void
IDDelete (const char *dev, const char *name, const char *msg, ...)
{
	printf ("<delProperty device='%s'", dev);
	if (name)
	    printf (" name='%s'", name);
	printf (">\n");

	if (msg) {
	    va_list ap;
	    printf (" <msg time='%s'>", timestamp());
	    va_start (ap, msg);
	    vprintf (msg, ap);
	    va_end (ap);
	    printf ("</msg>\n");
	}

	printf ("</delProperty>");
	fflush (stdout);
}

/* log message locally. Has nothing to do with any Clients. */
void
IDLog (const char *msg, ...)
{
	va_list ap;
	fprintf (stderr, "%s ", timestamp());
	va_start (ap, msg);
	vfprintf (stderr, msg, ap);
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

/* utility functions to find a member of a vector, or NULL if not found */

IText *
IUFindText  (const ITextVectorProperty *tp, const char *name)
{
	int i;

	for (i = 0; i < tp->nt; i++)
	    if (strcmp (tp->t[i].name, name) == 0)
		return (&tp->t[i]);
	fprintf (stderr, "No IText '%s' in %s.%s\n", name, tp->device,tp->name);
	return (NULL);
}

INumber *
IUFindNumber(const INumberVectorProperty *np, const char *name)
{
	int i;

	for (i = 0; i < np->nn; i++)
	    if (strcmp (np->n[i].name, name) == 0)
		return (&np->n[i]);
	fprintf (stderr, "No INumber '%s' in %s.%s\n",name,np->device,np->name);
	return (NULL);
}

ISwitch *
IUFindSwitch(const ISwitchVectorProperty *sp, const char *name)
{
	int i;

	for (i = 0; i < sp->nsw; i++)
	    if (strcmp (sp->sw[i].name, name) == 0)
		return (&sp->sw[i]);
	fprintf (stderr, "No ISwitch '%s' in %s.%s\n",name,sp->device,sp->name);
	return (NULL);
}

/* save a copy of newtext in tp->text.
 * N.B. this assumes tp->text is initially NULL and we are only ones to ever
 *   modify it.
 */
void
IUSaveText (IText *tp, const char *newtext)
{
	/* seed for realloc */
	if (tp->text == NULL)
	    tp->text = malloc (1);

	/* copy in fresh string */
	tp->text = strcpy (realloc (tp->text, strlen(newtext)+1), newtext);
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
	int i, n;

	if (verbose)
	    prXMLEle (stderr, root, 0);

	/* check tag in surmised decreasing order of likelyhood */

	if (!strcmp (root->tag, "newNumberVector")) {
	    static double *doubles;
	    static char **names;
	    static int maxn;
	    char *dev, *name;

	    /* pull out device and name */
	    if (crackDN (root, &dev, &name, msg) < 0)
		return (-1);

	    /* pull out each name/value pair */
	    for (n = i = 0; i < root->nel; i++) {
		XMLEle *ep = root->el[i];
		if (strcmp (ep->tag, "newNumber") == 0) {
		    XMLAtt *na = findXMLAtt (ep, "name");
		    if (na) {
			if (n >= maxn) {
			    int newsz = (maxn += 2)*sizeof(double);
			    doubles = doubles ? (double*)realloc(doubles,newsz)
			                     : (double *) malloc (newsz);
			    newsz = maxn*sizeof(char *);
			    names = names ? (char **) realloc (names, newsz)
			                  : (char **) malloc (newsz);
			}
			if (f_scansexa (ep->pcdata, &doubles[n]) < 0)
			    IDMessage (dev,"%s: Bad format %s",name,ep->pcdata);
			else
			{
			    IDLog("Receving a number %g for property %s\n", doubles[n], na->valu);
			    names[n++] = na->valu;
			}
		    }
		}
	    }

	    /* invoke driver if something to do, but not an error if not */
	    if (n > 0)
		ISNewNumber (dev, name, doubles, names, n);
	    else
		IDMessage (dev, "%s: set with no valid members", name);
	    return (0);
	}

	if (!strcmp (root->tag, "newSwitchVector")) {
	    static ISState *states;
	    static char **names;
	    static int maxn;
	    char *dev, *name;

	    /* pull out device and name */
	    if (crackDN (root, &dev, &name, msg) < 0)
		return (-1);

	    /* pull out each name/state pair */
	    for (n = i = 0; i < root->nel; i++) {
		XMLEle *ep = root->el[i];
		if (strcmp (ep->tag, "newSwitch") == 0) {
		    XMLAtt *na = findXMLAtt (ep, "name");
		    if (na) {
			if (n >= maxn) {
			    int newsz = (maxn += 2)*sizeof(ISState);
			    states = states ? (ISState *) realloc(states, newsz)
			            : (ISState *) malloc (newsz);
			    newsz = maxn*sizeof(char *);
			    names = names ? (char **) realloc (names, newsz)
			                  : (char **) malloc (newsz);
			}
			if (strcmp (ep->pcdata,"On") == 0) {
			    states[n] = ISS_ON;
			    names[n] = na->valu;
			    n++;
			} else if (strcmp (ep->pcdata,"Off") == 0) {
			    states[n] = ISS_OFF;
			    names[n] = na->valu;
			    n++;
			} else 
			    IDMessage (dev, "%s: must be On or Off: %s", name,
								    ep->pcdata);
		    }
		}
	    }

	    /* invoke driver if something to do, but not an error if not */
	    if (n > 0)
		ISNewSwitch (dev, name, states, names, n);
	    else
		IDMessage (dev, "%s: set with no valid members", name);
	    return (0);
	}

	if (!strcmp (root->tag, "newTextVector")) {
	    static char **texts;
	    static char **names;
	    static int maxn;
	    char *dev, *name;

	    /* pull out device and name */
	    if (crackDN (root, &dev, &name, msg) < 0)
		return (-1);

	    /* pull out each name/text pair */
	    for (n = i = 0; i < root->nel; i++) {
		XMLEle *ep = root->el[i];
		if (strcmp (ep->tag, "newText") == 0) {
		    XMLAtt *na = findXMLAtt (ep, "name");
		    if (na) {
			if (n >= maxn) {
			    int newsz = (maxn += 2)*sizeof(char *);
			    texts = texts ? (char **) realloc (texts, newsz)
			            : (char **) malloc (newsz);
			    newsz = maxn*sizeof(char *);
			    names = names ? (char **) realloc (names, newsz)
			                  : (char **) malloc (newsz);
			}
			texts[n] = ep->pcdata;
			names[n] = na->valu;
			n++;
		    }
		}
	    }

	    /* invoke driver if something to do, but not an error if not */
	    if (n > 0)
		ISNewText (dev, name, texts, names, n);
	    else
		IDMessage (dev, "%s: set with no valid members", name);
	    return (0);
	}

	if (!strcmp (root->tag, "getProperties")) {
	    XMLAtt *ap;
	    double v;

	    /* check version */
	    ap = findXMLAtt (root, "version");
	    if (!ap) {
		fprintf (stderr, "%s: getProperties missing version\n", me);
		exit(1);
	    }
	    v = atof (ap->valu);
	    if (v > INDIV) {
		fprintf (stderr, "%s: client version %g > %g\n", me, v, INDIV);
		exit(1);
	    }

	    /* ok */
	    ap = findXMLAtt (root, "device");
	    ISGetProperties (ap ? ap->valu : NULL);
	    return (0);
	}

	sprintf (msg, "Unknown command: %s", root->tag);
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
	    sprintf (msg, "%s requires 'device' attribute", root->tag);
	    return (-1);
	}
	*dev = ap->valu;

	ap = findXMLAtt (root, "name");
	if (!ap) {
	    sprintf (msg, "%s requires 'name' attribute", root->tag);
	    return (-1);
	}
	*name = ap->valu;

	return (0);
}

/* return static string corresponding to the given property or light state */
static char *
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
static char *
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
static char *
ruleStr (ISRule r)
{
	switch (r) {
	case ISR_1OFMANY: return ("OneOfMany");
	case ISR_NOFMANY: return ("AnyOfMany");
	default:
	    fprintf (stderr, "Impossible ISRule %d\n", r);
	    exit(1);
	}
}

/* return static string corresponding to the given IPerm */
static char *
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

/* return current system time in message format */
static char *
timestamp()
{
	static char ts[32];
	struct tm *tp;
	time_t t;

	time (&t);
	tp = gmtime (&t);
	strftime (ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", tp);
	return (ts);
}

/* fill buf with properly formatted INumber string. return length */
int
INumFormat (char *buf, INumber *ip)
{
        int w, f, s, l;

        if (sscanf (ip->format, "%%%d.%dm", &w, &f) == 2) {
            /* INDI sexi format */
            switch (f) {
            case 9:  s = 360000; break;
            case 8:  s = 36000;  break;
            case 6:  s = 3600;   break;
            case 5:  s = 600;    break;
            default: s = 60;     break;
            }
            l = fs_sexa (buf, ip->value, w-f, s);
        } else {
            /* normal printf format */
            l = sprintf (buf, ip->format, ip->value);
        }
        return (l);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile$ $Date$ $Revision$ $Name:  $"};
