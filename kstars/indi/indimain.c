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

/* main() for one INDI device driver process.
 * INDI XML commands arrive on stdin and are dispatched with IS*() funcs.
 * Devices call IC*() functions and INDI XML commands are sent out stdout.
 * Troubles are reported on stderr.
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

#include "indiapi.h"

static void usage(void);
static void driverRun(void);
static int dispatch (XMLEle *root, char msg[]);
static int crackDN (XMLEle *root, char **dev, char **name, char msg[]);
static char *lstateStr(ILState s);
static char *sstateStr(ISState s);
static char *ruleStr(IRule r);
static char *permStr(IPerm p);
static char *spermStr(ISPerm p);
static char *timestamp (void);

static int verbose;			/* chatty */
char *me;			/* a.out name */
static LilXML *clixml;			/* XML parser context */
static int devpoll;			/* ms to poll, none if 0 */

int
main (int ac, char *av[])
{
	/* save name */
	me = av[0];

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
	ISInit();

	/* service client */
	while (1)
	    driverRun();

	/* eh?? */
	fprintf (stderr, "%s: inf loop ended\n", me);
	return (1);
}

/* request calling ISPoll() every ms, or not at all if 0 */
void
ICPollMe (int ms)
{
	devpoll = abs(ms);
}

/* tell client to create a new text property */
void
ICDefText (IText *t, char *prompt, IPerm p)
{
	printf ("<defText device='%s' name='%s'", t->dev, t->name);
	printf (" perm='%s' state='%s'", permStr(p), lstateStr(t->s));
	printf (" timeout='%g'", t->tout);
	printf (" grouptag='%s'>\n", t->grouptag ? t->grouptag : "");
	printf (" <prompt>%s</prompt>\n", prompt);
	printf (" <text>%s</text>\n", t->text);
	printf ("</defText>\n");
	fflush (stdout);
}

/* tell client to create a new numeric property */
void
ICDefNumber (INumber *n, char *prompt, IPerm p, INRange *r)
{
	printf ("<defNumber device='%s' name='%s'", n->dev, n->name);
	printf (" perm='%s' state='%s'", permStr(p), lstateStr(n->s));
	printf (" timeout='%g'", n->tout);
	printf (" grouptag='%s'>\n", n->grouptag ? n->grouptag : "");
	if (r) {
	    printf (" <range>\n");
	    if (r->rSet & INR_MIN)
		printf ("   <min>%g</min>\n", r->min);
	    if (r->rSet & INR_MAX)
		printf ("   <max>%g</max>\n", r->max);
	    if (r->rSet & INR_STEP)
		printf ("   <step>%g</step>\n", r->step);
	    printf (" </range>\n");
	}
	printf (" <prompt>%s</prompt>\n", prompt);
	printf (" <number>%s</number>\n", n->nstr);
	printf ("</defNumber>\n");
	fflush (stdout);
}

/* tell client to create a new switches property */
void
ICDefSwitches (ISwitches *s, char *prompt, IPerm p, IRule r)
{
	int i;

	printf ("<defSwitches device='%s' name='%s'", s->dev, s->name);
	printf (" perm='%s' state='%s'", spermStr(p), lstateStr(s->s));
	printf (" rule='%s' timeout='%g'", ruleStr(r), s->tout);
	printf (" grouptag='%s'>\n", s->grouptag ? s->grouptag : "");
	printf (" <prompt>%s</prompt>\n", prompt);
	for (i = 0; i < s->nsw; i++)
	    printf (" <switch state='%s'>%s</switch>\n", sstateStr(s->sw[i].s),
								s->sw[i].name);
	printf ("</defSwitches>\n");
	fflush (stdout);
}

/* tell client to create a new lights property */
void
ICDefLights (ILights *l, char *prompt)
{
	int i;

	printf ("<defLights device='%s' name='%s'", l->dev, l->name);
	printf (" state='%s'", lstateStr(l->s));
	printf (" grouptag='%s'>\n", l->grouptag ? l->grouptag : "");
	printf (" <prompt>%s</prompt>\n", prompt);
	for (i = 0; i < l->nl; i++)
	    printf (" <light state='%s'>%s</light>\n", lstateStr(l->l[i].s),
								l->l[i].name);
	printf ("</defLights>\n");
	fflush (stdout);
}

/* tell client to update an existing text property */
void
ICSetText (IText *t, char *msg, ...)
{
	printf ("<setText device='%s' name='%s'", t->dev, t->name);
	printf (" timeout='%g' state='%s'>\n", t->tout, lstateStr(t->s));
	printf (" <text>%s</text>\n", t->text);
	if (msg) {
	    va_list ap;
	    printf (" <msg time='%s'>", timestamp());
	    va_start (ap, msg);
	    vprintf (msg, ap);
	    va_end (ap);
	    printf ("</msg>\n");
	}
	printf ("</setText>\n");
	fflush (stdout);
}

/* tell client to update an existing numeric property */
void
ICSetNumber (INumber *n, char *msg, ...)
{
	printf ("<setNumber device='%s' name='%s'", n->dev, n->name);
	printf (" timeout='%g' state='%s'>\n", n->tout, lstateStr(n->s));
	printf (" <number>%s</number>\n", n->nstr);
	if (msg) {
	    va_list ap;
	    printf (" <msg time='%s'>", timestamp());
	    va_start (ap, msg);
	    vprintf (msg, ap);
	    va_end (ap);
	    printf ("</msg>\n");
	}
	printf ("</setNumber>\n");
	fflush (stdout);
}

/* tell client to update an existing switch property */
void
ICSetSwitch (ISwitches *s, char *msg, ...)
{
	int i;

	printf ("<setSwitch device='%s' name='%s'", s->dev, s->name);
	printf (" timeout='%g' state='%s'>\n", s->tout, lstateStr(s->s));
	for (i = 0; i < s->nsw; i++)
	    printf (" <switch state='%s'>%s</switch>\n", sstateStr(s->sw[i].s),
								s->sw[i].name);
	if (msg) {
	    va_list ap;
	    printf (" <msg time='%s'>", timestamp());
	    va_start (ap, msg);
	    vprintf (msg, ap);
	    va_end (ap);
	    printf ("</msg>\n");
	}
	printf ("</setSwitch>\n");
	fflush (stdout);
}

/* tell client to update an existing lights property */
void
ICSetLights (ILights *l, char *msg, ...)
{
	int i;

	printf ("<setLights device='%s' name='%s'", l->dev, l->name);
	printf (" state='%s'>\n", lstateStr(l->s));
	for (i = 0; i < l->nl; i++)
	    printf (" <light state='%s'>%s</light>\n", lstateStr(l->l[i].s),
								l->l[i].name);
	if (msg) {
	    va_list ap;
	    printf (" <msg time='%s'>", timestamp());
	    va_start (ap, msg);
	    vprintf (msg, ap);
	    va_end (ap);
	    printf ("</msg>\n");
	}
	printf ("</setLights>\n");
	fflush (stdout);
}

/* send client a message for a specific device or at large if !dev */
void
ICMessage (char *dev, char *msg, ...)
{
	printf ("<message");
	if (dev)
	    printf (" device='%s'", dev);
	printf (">\n");
	if (msg) {
	    va_list ap;
	    printf (" <msg time='%s'>", timestamp());
	    va_start (ap, msg);
	    vprintf (msg, ap);
	    va_end (ap);
	    printf ("</msg>\n");
	}
	printf ("</message>");
	fflush (stdout);
}

/* tell Client to delete the property with given name on given device, or
 * entire device if !name
 */
void
ICDelete (char *dev, char *name, char *msg, ...)
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
ICLog (char *msg, ...)
{
	va_list ap;

	fprintf (stderr, "%s ", timestamp());
	va_start (ap, msg);
	vfprintf (stderr, msg, ap);
	va_end (ap);
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

/* check for client command and poll driver if desired */
static void
driverRun(void)
{
	struct timeval to, *tp;
	fd_set rs;
	int s;

	/* wait for client on stdin */
	FD_ZERO (&rs);
	FD_SET (0, &rs);

	/* wait just so long if device wants to be polled */
	if (devpoll > 0) {
	    to.tv_sec = devpoll/1000;
	    to.tv_usec = (1000*devpoll)%1000000;
	    tp = &to;
	} else {
	    /* wait forever for client command */
	    tp = NULL;
	}
	s = select (1, &rs, NULL, NULL, tp);
	if (s < 0) {
	    fprintf (stderr, "select: %s\n", strerror(errno));
	    exit(1);
	}
	if (s == 0) {
	    /* timed out, poll device */
	    ISPoll();
	} else if (FD_ISSET (0, &rs)) {
	    /* more client message waiting, collect and dispatch if complete */
	    char buf[1024], *bp;
	    int nr = read (0, buf, sizeof(buf));
	    if (nr < 0) {
		fprintf (stderr, "%s: %s\n", me, strerror(errno));
		exit(1);
	    }
	    if (nr == 0) {
		fprintf (stderr, "%s: EOF\n", me);
		exit(1);
	    }
	    for (bp = buf; nr-- > 0; bp++) {
		char msg[1024];
		XMLEle *root = readXMLEle (clixml, *bp, msg);
		if ((!root && msg[0]) || (root && dispatch (root, msg) < 0))
		    fprintf (stderr, "%s: %s\n", me, msg);
		if (root)
		    delXMLEle (root);
	    }
	}
}

/* crack the given INDI XML element and call driver's entry points.
 * return 0 if else -1 with reason in msg[].
 * N.B. except: exit if getProperties does not claim compatible version.
 */
static int
dispatch (XMLEle *root, char msg[])
{
	if (verbose)
	    fprintf (stderr, "dispatch %s\n", root->tag);

	/* check tag in roughly decreasing order of likelyhood */

	if (!strcmp (root->tag, "newNumber")) {
	    char *dev, *name;
	    XMLEle *ep;
	    INumber n;

	    if (crackDN (root, &dev, &name, msg) < 0)
		return (-1);
	    ep = findXMLEle (root, "number");
	    if (!ep) {
		sprintf (msg, "%s requires 'number' child element", root->tag);
		return (-1);
	    }
	    n.dev = dev;
	    n.name = name;
	    n.nstr = ep->pcdata;

	    ISNewNumber (&n);
	    return (0);
	}

	if (!strcmp (root->tag, "newSwitch")) {
	    char *dev, *name;
	    XMLEle *ep;
	    XMLAtt *ap;
	    ISwitches s;
	    ISwitch sw;

	    if (crackDN (root, &dev, &name, msg) < 0)
		return (-1);
	    ep = findXMLEle (root, "switch");
	    if (!ep) {
		sprintf (msg, "%s requires 'switch' child element", root->tag);
		return (-1);
	    }
	    ap = findXMLAtt (ep, "state");
	    if (!ap) {
		sprintf (msg, "%s switch requires 'state' attribute",root->tag);
		return (-1);
	    }

	    sw.name = ep->pcdata;
	    if (!strcmp (ap->valu, "On"))
		sw.s = ISS_ON;
	    else if (!strcmp (ap->valu, "Off"))
		sw.s = ISS_OFF;
	    else {
		sprintf (msg, "%s switch state must be On or Off", root->tag);
		return (-1);
	    }
	    s.dev = dev;
	    s.name = name;
	    s.sw = &sw;

	    ISNewSwitch (&s);

	    return (0);
	}

	if (!strcmp (root->tag, "newText")) {
	    char *dev, *name;
	    XMLEle *ep;
	    IText t;

	    if (crackDN (root, &dev, &name, msg) < 0)
		return (-1);
	    ep = findXMLEle (root, "text");
	    if (!ep) {
		sprintf (msg, "%s requires 'text' child element", root->tag);
		return (-1);
	    }
	    t.dev = dev;
	    t.name = name;
	    t.text = ep->pcdata;

	    ISNewText (&t);
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

/* return static string corresponding to the given light state */
static char *
lstateStr (ILState s)
{
	switch (s) {
	case ILS_IDLE:  return ("Idle");
	case ILS_OK:    return ("Ok");
	case ILS_BUSY:  return ("Busy");
	case ILS_ALERT: return ("Alert");
	default:
	    fprintf (stderr, "Impossible ILState %d\n", s);
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
ruleStr (IRule r)
{
	switch (r) {
	case IR_1OFMANY: return ("OneOfMany");
	case IR_NOFMANY: return ("AnyOfMany");
	default:
	    fprintf (stderr, "Impossible IRule %d\n", r);
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

/* return static string corresponding to the given ISPerm */
static char *
spermStr (ISPerm p)
{
	switch (p) {
	case ISP_R: return ("r");
	case ISP_W: return ("w");
	default:
	    fprintf (stderr, "Impossible ISPerm %d\n", p);
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

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile$ $Date$ $Revision$ $Name:  $"};
