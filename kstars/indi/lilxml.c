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
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

#endif

/* little XML parser.
 * only handles elements, attributes and content.
 * <! ... > and <? ... > are silently ignored.
 * all content is collected into one string, sans leading whitespace first line.
 * attribute name and valu and element tag and pcdata are always at least "".
 *
 * #define MAIN_TST to create standalone test program
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "lilxml.h"

static int oneXMLchar (LilXML *lp, int c, char errmsg[]);
static void initParser(LilXML *lp);
static void pushXMLEle(LilXML *lp);
static void popXMLEle(LilXML *lp);
static void resetEndTag(LilXML *lp);
static void addAttr(LilXML *lp);
static void delAttr (XMLAtt *a);
static int isTokenChar (int start, int c);
static void growString (char **sp, char c);
static void growPCData (XMLEle *ep, char c);
static char *newString (void);
static void *moremem (void *old, int n);

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
    INCLOSETAG,				/* reading closing tag */
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

/* pass back a fresh handle for use with our other functions */
LilXML *
newLilXML ()
{
	LilXML *lp = (LilXML *) calloc (1, sizeof(LilXML));
	initParser(lp);
	return (lp);
}

/* discard */
void
delLilXML (LilXML *lp)
{
	free (lp);
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
	    sprintf (errmsg, "Line %d: XML EOF", lp->ln);
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

/* delete ep and all its children */
void
delXMLEle (XMLEle *ep)
{
	int i;

	/* benign if NULL */
	if (!ep)
	    return;

	/* delete all parts of ep */
	free (ep->tag);
	free (ep->pcdata);
	if (ep->at) {
	    for (i = 0; i < ep->nat; i++)
		delAttr (ep->at[i]);
	    free (ep->at);
	}
	if (ep->el) {
	    for (i = 0; i < ep->nel; i++)
		delXMLEle (ep->el[i]);
	    free (ep->el);
	}

	/* delete ep itself */
	free (ep);
}

/* search ep for an attribute with given name.
 * return NULL if not found.
 */
XMLAtt *
findXMLAtt (XMLEle *ep, char *name)
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
findXMLEle (XMLEle *ep, char *tag)
{
	int i;

	for (i = 0; i < ep->nel; i++)
	    if (!strcmp (ep->el[i]->tag, tag))
		return (ep->el[i]);
	return (NULL);
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
		sprintf (errmsg, "Line %d: Bogus tag char %c", lp->ln, c);
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
		sprintf (errmsg, "Line %d: Bogus 1st attr name char: %c",lp->ln,
									    c);
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
		sprintf (errmsg, "Line %d: Bogus char %c before >", lp->ln, c);
		return (-1);
	    }
	    break;
		
	case INATTRN:			/* reading attr name */
	    if (isTokenChar (0, c))
		growString (&lp->ce->at[lp->ce->nat-1]->name, c);
	    else if (isspace(c) || c == '=')
		lp->cs = LOOK4ATTRV;
	    else {
		sprintf (errmsg, "Line %d: Bogus attr name char: %c", lp->ln,c);
		return (-1);
	    }
	    break;

	case LOOK4ATTRV:		/* looking for attr value */
	    if (c == '\'' || c == '"') {
		lp->delim = c;
		growString (&lp->ce->at[lp->ce->nat-1]->valu, '\0');
		lp->cs = INATTRV;
	    } else if (!(isspace(c) || c == '=')) {
		sprintf (errmsg, "Line %d: No attribute value", lp->ln);
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
		sprintf (errmsg, "Line %d: Bogus preend tag char %c", lp->ln,c);
		return (-1);
	    }
	    break;

	case INCLOSETAG:		/* reading closing tag */
	    if (isTokenChar(0, c))
		growString (&lp->endtag, c);
	    else if (c == '>') {
		if (strcmp (lp->ce->tag, lp->endtag)) {
		    sprintf (errmsg,"Line %d: closing tag %s does not match %s",
					    lp->ln, lp->endtag, lp->ce->tag);
		    return (-1);
		} else if (lp->ce->pe) {
		    popXMLEle(lp);
		    lp->cs = LOOK4CON;	/* back to content after nested elem */
		} else
		    return (1);		/* yes! */
	    } else if (!isspace(c)) {
		sprintf (errmsg, "Line %d: Bogus end tag char %c", lp->ln, c);
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
	XMLEle *newe = (XMLEle *) calloc (1, sizeof(XMLEle));
	XMLEle *ce = lp->ce;

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
	XMLAtt *newa = (XMLAtt *) calloc (1, sizeof(XMLAtt));
	XMLEle *ce = lp->ce;

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
	    free (a->name);
	if (a->valu)
	    free (a->valu);
	free(a);
}

/* delete endtag if appropriate */
static void
resetEndTag(LilXML *lp)
{
	if (lp->endtag) {
	    free (lp->endtag);
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
growString (char **sp, char c)
{
	int l = *sp ? strlen(*sp) : 0;
	*sp = (char *) moremem (*sp, l+2);	/* c + '\0' */
	(*sp)[l++] = c;
	(*sp)[l] = '\0';
}

/* special fast version of growString just for ep->pcdata that avoids all the
 * strlens and tiny increments in allocated mem
 */
static void
growPCData (XMLEle *ep, char c)
{
	int l = ep->pcdatal++;
	if ((l%32) == 0) {
	    int nm = 32*(l/32+1) + 2;               /* c + '\0' */
	    ep->pcdata = realloc (ep->pcdata, nm);
	}
	ep->pcdata[l++] = c;
	ep->pcdata[l] = '\0';
}

/* return a malloced string of one '\0' */
static char *
newString()
{
	char *str;
	
	*(str = (char *)malloc(16)) = '\0';	/* expect more */
	return (str);
}

static void *
moremem (void *old, int n)
{
	return (old ? realloc (old, n) : malloc (n));
}

#if defined(MAIN_TST)
int
main (int ac, char *av[])
{
	LilXML *lp = newLilXML();
	char errmsg[1024];
	XMLEle *root;
	int c;

	while ((c = fgetc(stdin)) != EOF) {
	    root = readXMLEle (lp, c, errmsg);
	    if (root) {
		fprintf (stderr, "::::::::::::: %s\n", root->tag);
		prXMLEle (stdout, root, 0);
		delXMLEle (root);
	    } else if (errmsg[0]) {
		fprintf (stderr, "Error: %s\n", errmsg);
	    }
	}

	delLilXML (lp);

	return (0);
}
#endif

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile$ $Date$ $Revision$ $Name:  $"};
