#if 0
    liblilxml
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

/* little DOM-style XML parser.
 * only handles elements, attributes and pcdata content.
 * <! ... > and <? ... > are silently ignored.
 * pcdata is collected into one string, sans leading whitespace first line.
 *
 * #define MAIN_TST to create standalone test program
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "lilxml.h"
#include "indicom.h"

static int oneXMLchar (LilXML *lp, int c, char errmsg[]);
static void initParser(LilXML *lp);
static void pushXMLEle(LilXML *lp);
static void popXMLEle(LilXML *lp);
static void resetEndTag(LilXML *lp);
static void addAttr(LilXML *lp);
static void delAttr (XMLAtt *a);
static int isTokenChar (int start, int c);
static void growString (char **sp, int c);
static void growPCData (XMLEle *ep, int c);
static char *newString (void);
static void *moremem (void *old, int n);
static void freemem (void *m);

typedef enum  {
    LOOK4START = 0,			/* looking for first element start */
    LOOK4TAG,				/* looking for element tag */
    INTAG,				/* reading tag */
    LOOK4ATTRN,				/* looking for attr name, > or / */
    INATTRN,				/* reading attr name */
    LOOK4ATTRV,				/* looking for attr value */
    SAWSLASH,				/* saw / in element opening */
    INATTRV,				/* in attr value */
    LOOK4CON,				/* skipping leading content whitespc */
    INCON,				/* reading content */
    LTINCON,				/* saw < in content */
    LOOK4CLOSETAG,			/* looking for closing tag after < */
    INCLOSETAG				/* reading closing tag */
} State;				/* parsing states */

/* maintain state while parsing */
struct _LilXML {
    State cs;				/* current state */
    int ln;				/* line number for diags */
    XMLEle *ce;				/* current element being built */
    char *endtag;			/* to check for match with opening tag*/
    int delim;				/* attribute value delimiter */
    int lastc;				/* last char (just used wiht skipping)*/
    int skipping;			/* in comment or declaration */
};

/* internal representation of a (possibly nested) XML element */
struct _xml_ele {
    char *tag;			/* element tag */
    struct _xml_ele *pe;	/* parent element, or NULL if root */
    XMLAtt **at;		/* list of attributes */
    int nat;			/* number of attributes */
    int ait;			/* used to iterate over at[] */
    struct _xml_ele **el;	/* list of child elements */
    int nel;			/* number of child elements */
    int eit;			/* used to iterate over el[] */
    char *pcdata;		/* character data in this element */
    int pcdatal;		/* handy length sans \0 (tends to be big) */
};

/* internal representation of an attribute */
struct _xml_att {
    char *name;			/* name */
    char *valu;			/* value */
    struct _xml_ele *ce;	/* containing element */
};

/* default memory managers, override with indi_xmlMalloc() */
static void *(*mymalloc)(size_t size) = malloc;
static void *(*myrealloc)(void *ptr, size_t size) = realloc;
static void (*myfree)(void *ptr) = free;

/* install new version of malloc/realloc/free.
 * N.B. don't call after first use of any other lilxml function
 */
void
indi_xmlMalloc (void *(*newmalloc)(size_t size),
	   void *(*newrealloc)(void *ptr, size_t size),
	   void (*newfree)(void *ptr))
{
	mymalloc = newmalloc;
	myrealloc = newrealloc;
	myfree = newfree;
}

/* pass back a fresh handle for use with our other functions */
LilXML *
newLilXML ()
{
	LilXML *lp = (LilXML *) moremem (NULL, sizeof(LilXML));
	initParser(lp);
	return (lp);
}

/* discard */
void
delLilXML (LilXML *lp)
{
	freemem (lp);
}

/* delete ep and all its children */
void
delXMLEle (XMLEle *ep)
{
	int i;

	/* benign if NULL */
	if (!ep)
	    return;

	/* delete all parts of ep */
	freemem (ep->tag);
	freemem (ep->pcdata);
	if (ep->at) {
	    for (i = 0; i < ep->nat; i++)
		delAttr (ep->at[i]);
	    freemem (ep->at);
	}
	if (ep->el) {
	    for (i = 0; i < ep->nel; i++)
		delXMLEle (ep->el[i]);
	    freemem (ep->el);
	}

	/* delete ep itself */
	freemem (ep);
}

/* process one more character of an XML file.
 * when find closure with outter element return root of complete tree.
 * when find error return NULL with reason in errmsg[].
 * when need more return NULL with errmsg[0] = '\0'.
 * N.B. it is up to the caller to delete the tree delXMLEle().
 */
XMLEle *
readXMLEle (LilXML *lp, int newc, char errmsg[])
{
	XMLEle *root;
	int s;

	/* start optimistic */
	errmsg[0] = '\0';

	/* EOF? */
	if (newc == 0) {
	    snprintf (errmsg, ERRMSG_SIZE, "Line %d: XML EOF", lp->ln);
	    initParser(lp);
	    return (NULL);
	}

	/* new line? */
	if (newc == '\n')
	    lp->ln++;

	/* skip comments and declarations. requires 1 char history */
	if (!lp->skipping && lp->lastc == '<' && (newc == '?' || newc == '!')) {
	    lp->skipping = 1;
	    lp->lastc = newc;
	    return (NULL);
	}
	if (lp->skipping) {
	    if (newc == '>')
		lp->skipping = 0;
	    lp->lastc = newc;
	    return (NULL);
	}
	if (newc == '<') {
	    lp->lastc = '<';
	    return (NULL);
	}

	/* do a pending '<' first then newc */
	if (lp->lastc == '<') {
	    if (oneXMLchar (lp, '<', errmsg) < 0) {
		initParser(lp);
		return (NULL);
	    }
	    /* N.B. we assume '<' will never result in closure */
	}

	/* process newc (at last!) */
	s = oneXMLchar (lp, newc, errmsg);
	if (s == 0) {
	    lp->lastc = newc;
	    return (NULL);
	}
	if (s < 0) {
	    initParser(lp);
	    return (NULL);
	}

	/* Ok! return ce and we start over.
	 * N.B. up to caller to call delXMLEle with what we return.
	 */
	root = lp->ce;
	lp->ce = NULL;
	initParser(lp);
	return (root);
}

/* search ep for an attribute with given name.
 * return NULL if not found.
 */
XMLAtt *
findXMLAtt (XMLEle *ep, const char *name)
{
	int i;

	for (i = 0; i < ep->nat; i++)
	    if (!strcmp (ep->at[i]->name, name))
		return (ep->at[i]);
	return (NULL);
}

/* search ep for an element with given tag.
 * return NULL if not found.
 */
XMLEle *
findXMLEle (XMLEle *ep, const char *tag)
{
	int i;

	for (i = 0; i < ep->nel; i++)
	    if (!strcmp (ep->el[i]->tag, tag))
		return (ep->el[i]);
	return (NULL);
}

/* iterate over each child element of ep.
 * call first time with first set to 1, then 0 from then on.
 * returns NULL when no more or err
 */
XMLEle *
nextXMLEle (XMLEle *ep, int init)
{
	int eit;
	
	if (init)
	    ep->eit = 0;

	eit = ep->eit++;
	if (eit < 0 || eit >= ep->nel)
	    return (NULL);
	return (ep->el[eit]);
}

/* iterate over each attribute of ep.
 * call first time with first set to 1, then 0 from then on.
 * returns NULL when no more or err
 */
XMLAtt *
nextXMLAtt (XMLEle *ep, int init)
{
	int ait;

	if (init)
	    ep->ait = 0;

	ait = ep->ait++;
	if (ait < 0 || ait >= ep->nat)
	    return (NULL);
	return (ep->at[ait]);
}

/* return parent of given XMLEle */
XMLEle *
parentXMLEle (XMLEle *ep)
{
	return (ep->pe);
}

/* return parent element of given XMLAtt */
XMLEle *
parentXMLAtt (XMLAtt *ap)
{
	return (ap->ce);
}

/* access functions */

/* return the tag name of the given element */
char *
tagXMLEle (XMLEle *ep)
{
	return (ep->tag);
}

/* return the pcdata portion of the given element */
char *
pcdataXMLEle (XMLEle *ep)
{
	return (ep->pcdata);
}

/* return the number of characters in the pcdata portion of the given element */
int 
pcdatalenXMLEle (XMLEle *ep)
{
	return (ep->pcdatal);
}

/* return the nanme of the given attribute */
char *
nameXMLAtt (XMLAtt *ap)
{
	return (ap->name);
}

/* return the value of the given attribute */
char *
valuXMLAtt (XMLAtt *ap)
{
	return (ap->valu);
}

/* return the number of child elements of the given element */
int
nXMLEle (XMLEle *ep)
{
	return (ep->nel);
}

/* return the number of attributes in the given element */
int
nXMLAtt (XMLEle *ep)
{
	return (ep->nat);
}


/* search ep for an attribute with the given name and return its value.
 * return "" if not found.
 */
const char *
findXMLAttValu (XMLEle *ep, char *name)
{
	XMLAtt *a = findXMLAtt (ep, name);
	return (a ? a->valu : "");
}

/* handy wrapper to read one xml file.
 * return root element else NULL with report in errmsg[]
 */
XMLEle *
readXMLFile (FILE *fp, LilXML *lp, char errmsg[])
{
	int c;

	while ((c = fgetc(fp)) != EOF) {
	    XMLEle *root = readXMLEle (lp, c, errmsg);
	    if (root || errmsg[0])
		return (root);
	}

	return (NULL);
}

/* sample print ep to fp
 * N.B. set level = 0 on first call
 */
#define	PRINDENT	4		/* sample print indent each level */
void
prXMLEle (FILE *fp, XMLEle *ep, int level)
{
	int indent = level*PRINDENT;
	int i;

	fprintf (fp, "%*s<%s", indent, "", ep->tag);
	for (i = 0; i < ep->nat; i++)
	    fprintf (fp, " %s=\"%s\"", ep->at[i]->name, ep->at[i]->valu);
	if (ep->nel > 0) {
	    fprintf (fp, ">\n");
	    for (i = 0; i < ep->nel; i++)
		prXMLEle (fp, ep->el[i], level+1);
	}
	if (ep->pcdata[0]) {
	    char *nl;
	    if (ep->nel == 0)
		fprintf (fp, ">\n");
	    /* indent if none or one line */
	    nl = strpbrk (ep->pcdata, "\n\r");
	    if (!nl || nl == &ep->pcdata[ep->pcdatal-1])
		fprintf (fp, "%*s", indent+PRINDENT, "");
	    fprintf (fp, "%s", ep->pcdata);
	    if (!nl)
		fprintf (fp, "\n");
	}
	if (ep->nel > 0 || ep->pcdata[0])
	    fprintf (fp, "%*s</%s>\n", indent, "", ep->tag);
	else
	    fprintf (fp, "/>\n");
}



/* process one more char in XML file.
 * if find final closure, return 1 and tree is in ce.
 * if need more, return 0.
 * if real trouble, return -1 and put reason in errmsg.
 */
static int
oneXMLchar (LilXML *lp, int c, char errmsg[])
{
	switch (lp->cs) {
	case LOOK4START:		/* looking for first element start */
	    if (c == '<') {
		pushXMLEle(lp);
		lp->cs = LOOK4TAG;
	    }
	    /* silently ignore until resync */
	    break;

	case LOOK4TAG:			/* looking for element tag */
	    if (isTokenChar (1, c)) {
		growString (&lp->ce->tag, c);
		lp->cs = INTAG;
	    } else if (!isspace(c)) {
		snprintf (errmsg, ERRMSG_SIZE, "Line %d: Bogus tag char %c", lp->ln, c);
		return (-1);
	    }
	    break;
		
	case INTAG:			/* reading tag */
	    if (isTokenChar (0, c))
		growString (&lp->ce->tag, c);
	    else if (c == '>')
		lp->cs = LOOK4CON;
	    else if (c == '/')
		lp->cs = SAWSLASH;
	    else 
		lp->cs = LOOK4ATTRN;
	    break;

	case LOOK4ATTRN:		/* looking for attr name, > or / */
	    if (c == '>')
		lp->cs = LOOK4CON;
	    else if (c == '/')
		lp->cs = SAWSLASH;
	    else if (isTokenChar (1, c)) {
		addAttr(lp);
		growString (&lp->ce->at[lp->ce->nat-1]->name, c);
		lp->cs = INATTRN;
	    } else if (!isspace(c)) {
		snprintf (errmsg, ERRMSG_SIZE, "Line %d: Bogus leading attr name char: %c",
								    lp->ln, c);
		return (-1);
	    }
	    break;

	case SAWSLASH:			/* saw / in element opening */
	    if (c == '>') {
		if (!lp->ce->pe)
		    return(1);		/* root has no content */
		popXMLEle(lp);
		lp->cs = LOOK4CON;
	    } else {
		snprintf (errmsg, ERRMSG_SIZE, "Line %d: Bogus char %c before >", lp->ln, c);
		return (-1);
	    }
	    break;
		
	case INATTRN:			/* reading attr name */
	    if (isTokenChar (0, c))
		growString (&lp->ce->at[lp->ce->nat-1]->name, c);
	    else if (isspace(c) || c == '=')
		lp->cs = LOOK4ATTRV;
	    else {
		snprintf (errmsg, ERRMSG_SIZE, "Line %d: Bogus attr name char: %c", lp->ln,c);
		return (-1);
	    }
	    break;

	case LOOK4ATTRV:		/* looking for attr value */
	    if (c == '\'' || c == '"') {
		lp->delim = c;
		growString (&lp->ce->at[lp->ce->nat-1]->valu, '\0');
		lp->cs = INATTRV;
	    } else if (!(isspace(c) || c == '=')) {
		snprintf (errmsg, ERRMSG_SIZE, "Line %d: No value for attribute %.100s", lp->ln,
					    lp->ce->at[lp->ce->nat-1]->name);
		return (-1);
	    }
	    break;

	case INATTRV:			/* in attr value */
	    if (c == lp->delim)
		lp->cs = LOOK4ATTRN;
	    else if (!iscntrl(c))
		growString (&lp->ce->at[lp->ce->nat-1]->valu, c);
	    break;

	case LOOK4CON:			/* skipping leading content whitespace*/
	    if (c == '<')
		lp->cs = LTINCON;
	    else if (!isspace(c)) {
		growPCData (lp->ce, c);
		lp->cs = INCON;
	    }
	    break;

	case INCON:			/* reading content */
	    if (c == '<') {
		/* if text contains a nl trim trailing blanks.
		 * chomp trailing nl if only one.
		 */
		char *nl = strpbrk (lp->ce->pcdata, "\n\r");
		if (nl)
		    while (lp->ce->pcdatal > 0 &&
				    lp->ce->pcdata[lp->ce->pcdatal-1] == ' ')
			lp->ce->pcdata[--lp->ce->pcdatal] = '\0';
		if (nl == &lp->ce->pcdata[lp->ce->pcdatal-1])
		    lp->ce->pcdata[--lp->ce->pcdatal] = '\0'; /* safe! */
		lp->cs = LTINCON;
	    } else
		growPCData (lp->ce, c);
	    break;

	case LTINCON:			/* saw < in content */
	    if (c == '/') {
		resetEndTag(lp);
		lp->cs = LOOK4CLOSETAG;
	    } else {
		pushXMLEle(lp);
		if (isTokenChar(1,c)) {
		    growString (&lp->ce->tag, c);
		    lp->cs = INTAG;
		} else
		    lp->cs = LOOK4TAG;
	    }
	    break;

	case LOOK4CLOSETAG:		/* looking for closing tag after < */
	    if (isTokenChar (1, c)) {
		growString (&lp->endtag, c);
		lp->cs = INCLOSETAG;
	    } else if (!isspace(c)) {
		snprintf (errmsg, ERRMSG_SIZE, "Line %d: Bogus preend tag char %c", lp->ln,c);
		return (-1);
	    }
	    break;

	case INCLOSETAG:		/* reading closing tag */
	    if (isTokenChar(0, c))
		growString (&lp->endtag, c);
	    else if (c == '>') {
		if (strcmp (lp->ce->tag, lp->endtag)) {
		    snprintf (errmsg, ERRMSG_SIZE, "Line %d: closing tag %.64s does not match %.64s",
					    lp->ln, lp->endtag, lp->ce->tag);
		    return (-1);
		} else if (lp->ce->pe) {
		    popXMLEle(lp);
		    lp->cs = LOOK4CON;	/* back to content after nested elem */
		} else
		    return (1);		/* yes! */
	    } else if (!isspace(c)) {
		snprintf (errmsg, ERRMSG_SIZE, "Line %d: Bogus end tag char %c", lp->ln, c);
		return (-1);
	    }
	    break;
	}

	return (0);
}

/* set up for a fresh start */
static void
initParser(LilXML *lp)
{
	memset (lp, 0, sizeof(*lp));
	lp->cs = LOOK4START;
	lp->ln = 1;
	delXMLEle (lp->ce);
	lp->ce = NULL;
	resetEndTag(lp);
	lp->lastc = 0;
	lp->skipping = 0;
}

/* start a new XMLEle.
 * if ce already set up, add to its list of child elements.
 * point ce to a new XMLEle.
 * endtag no longer valid.
 */
static void
pushXMLEle(LilXML *lp)
{
	XMLEle *newe = (XMLEle *) moremem (NULL, sizeof(XMLEle));
	XMLEle *ce = lp->ce;

	memset (newe, 0, sizeof(*newe));
	newe->tag = newString();
	newe->pcdata = newString();
	newe->pe = ce;

	if (ce) {
	    ce->el = (XMLEle **) moremem (ce->el, (ce->nel+1)*sizeof(XMLEle*));
	    ce->el[ce->nel++] = newe;
	}
	lp->ce = newe;
	resetEndTag(lp);
}

/* point ce to parent of current ce.
 * endtag no longer valid.
 */
static void
popXMLEle(LilXML *lp)
{
	lp->ce = lp->ce->pe;
	resetEndTag(lp);
}

/* add one new XMLAtt to the current element */
static void
addAttr(LilXML *lp)
{
	XMLAtt *newa = (XMLAtt *) moremem (NULL, sizeof(XMLAtt));
	XMLEle *ce = lp->ce;

	memset (newa, 0, sizeof(*newa));
	newa->name = newString();
	newa->valu = newString();
	newa->ce = ce;

	ce->at = (XMLAtt **) moremem (ce->at, (ce->nat+1)*sizeof(XMLAtt *));
	ce->at[ce->nat++] = newa;
}

/* delete a and all it holds */
static void
delAttr (XMLAtt *a)
{
	if (!a)
	    return;
	if (a->name)
	    freemem (a->name);
	if (a->valu)
	    freemem (a->valu);
	freemem(a);
}

/* delete endtag if appropriate */
static void
resetEndTag(LilXML *lp)
{
	if (lp->endtag) {
	    freemem (lp->endtag);
	    lp->endtag = 0;
	}
}

/* 1 if c is a valid token character, else 0.
 * it can be alpha or '_' or numeric unless start.
 */
static int
isTokenChar (int start, int c)
{
	return (isalpha(c) || c == '_' || (!start && isdigit(c)));
}

/* grow the malloced string at *sp to append c */
static void
growString (char **sp, int c)
{
	int l = *sp ? strlen(*sp) : 0;
	*sp = (char *) moremem (*sp, l+2);	/* c + '\0' */
	(*sp)[l++] = (char)c;
	(*sp)[l] = '\0';
}

/* special fast version of growString just for ep->pcdata that avoids all the
 * strlens and tiny increments in allocated mem
 */
static void
growPCData (XMLEle *ep, int c)
{
	int l = ep->pcdatal++;
	if ((l%32) == 0) {
	    int nm = 32*(l/32+1) + 2;               /* c + '\0' */
	    ep->pcdata = (char *) moremem (ep->pcdata, nm);
	}
	ep->pcdata[l++] = (char)c;
	ep->pcdata[l] = '\0';
}

/* return a malloced string of one '\0' */
static char *
newString()
{
	char *str;
	
	*(str = (char *)moremem(NULL, 16)) = '\0';	/* expect more */
	return (str);
}

static void *
moremem (void *old, int n)
{
	return (old ? realloc (old, n) : malloc (n));
}

static void
freemem (void *m)
{
	free (m);
}

#if defined(MAIN_TST)
int
main (int ac, char *av[])
{
	LilXML *lp = newLilXML();
	char errmsg[ERRMSG_SIZE];
	XMLEle *root;

	root = readXMLFile (stdin, lp, errmsg);
	if (root) {
	    fprintf (stderr, "::::::::::::: %s\n", tagXMLEle(root));
	    prXMLEle (stdout, root, 0);
	    delXMLEle (root);
	} else if (errmsg[0]) {
	    fprintf (stderr, "Error: %s\n", errmsg);
	}

	delLilXML (lp);

	return (0);
}
#endif

