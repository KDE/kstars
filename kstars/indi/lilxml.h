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

#ifndef LILXML_H
#define LILXML_H

/* public representation of a (possibly nested) XML element */
typedef struct {
    char *name;			/* name */
    char *valu;			/* value */
    struct _xml_ele *ce;	/* containing element */
} XMLAtt;

typedef struct _xml_ele {
    char *tag;			/* element tag */
    struct _xml_ele *pe;	/* parent element, or NULL if root */
    XMLAtt **at;		/* list of attributes */
    int nat;			/* number of attributes */
    struct _xml_ele **el;	/* list of child elements */
    int nel;			/* number of child elements */
    char *pcdata;		/* character data in this element */
    int pcdatal;		/* handy length sans \0 (tends to be big) */
} XMLEle;

/* opaque parsing handle */
typedef struct _LilXML LilXML;

/* parsing functions.
 * canonical usage pattern:

	LilXML *lp = newLilXML();
	char errmsg[1024];
	XMLEle *root;
	int c;

	while ((c = fgetc(stdin)) != EOF) {
	    root = readXMLEle (lp, c, errmsg);
	    if (root) {
		fprintf (stderr, "::::::::::::: %s\n", root->tag);
		prXMLEle (root, 0);
		delXMLEle (root);
	    } else if (errmsg[0]) {
		fprintf (stderr, "Error: %s\n", errmsg);
	    }
	}

	delLilXML (lp);
 */
extern LilXML *newLilXML(void);
extern void delLilXML (LilXML *lp);
extern XMLEle *readXMLEle (LilXML *l, int c, char errmsg[]);
extern void delXMLEle (XMLEle *e);
extern void prXMLEle (FILE *fp, XMLEle *e, int level);

/* search functions */
extern XMLAtt *findXMLAtt (XMLEle *e, char *name);
extern XMLEle *findXMLEle (XMLEle *e, char *tag);

/* For RCS Only -- Do Not Edit
 * @(#) $RCSfile$ $Date$ $Revision$ $Name:  $
 */

#endif	/* LILXML_H */
