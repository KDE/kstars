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

/** \file lilxml.h
    \brief A little DOM-style library to handle parsing and processing an XML file.
    
    It only handles elements, attributes and pcdata content. <! ... > and <? ... > are silently ignored. pcdata is collected into one string, sans leading whitespace first line. \n
    
    The following is an example of a cannonical usage for the lilxml library. Initialize a lil xml context and read an XML file in a root element.
    
    \code
    
    #include <lilxml.h>
    
    LilXML *lp = newLilXML();
	char errmsg[1024];
	XMLEle *root, *ep;
	int c;

	while ((c = fgetc(stdin)) != EOF) {
	    root = readXMLEle (lp, c, errmsg);
	    if (root)
		break;
	    if (errmsg[0])
		error ("Error: %s\n", errmsg);
	}
 
        // print the tag and pcdata content of each child element within the root 

        for (ep = nextXMLEle (root, 1); ep != NULL; ep = nextXMLEle (root, 0))
	    printf ("%s: %s\n", tagXMLEle(ep), pcdataXMLEle(ep));


	// finished with root element and with lil xml context

	delXMLEle (root);
	delLilXML (lp);
	
     \endcode
    
 */

#ifndef LILXML_H
#define LILXML_H

#ifdef __cplusplus
extern "C" {
#endif

/* opaque handle types */
typedef struct _xml_att XMLAtt;
typedef struct _xml_ele XMLEle;
typedef struct _LilXML LilXML;

/**
 * \defgroup lilxmlFunctions Functions to parse, process, and search XML.
 */
/*@{*/

/* creation and destruction functions */

/** \brief Create a new lilxml parser.
    \return a pointer to the lilxml parser on sucess. NULL on failure.
*/
extern LilXML *newLilXML(void);

/** \brief Delete a lilxml parser.
    \param lp a pointer to a lilxml parser to be deleted.
*/
extern void delLilXML (LilXML *lp);

/** \brief Delete an XML element.
    \return a pointer to the XML Element to be deleted.
*/
extern void delXMLEle (XMLEle *e);

/** \brief Process an XML one char at a time.
    \param lp a pointer to a lilxml parser.
    \param c one character to process.
    \param errmsg a buffer to store error messages if an error in parsing is encounterd.
    \return When the function parses a complete valid XML element, it will return a pointer to the XML element. A NULL is returned when parsing the element is still in progress, or if a parsing error occurs. Check errmsg for errors if NULL is returned. 
 */
extern XMLEle *readXMLEle (LilXML *lp, int c, char errmsg[]);

/* search functions */
/** \brief Find an XML attribute within an XML element.
    \param e a pointer to the XML element to search.
    \param name the attribute name to search for.
    \return A pointer to the XML attribute if found or NULL on failure.
*/
extern XMLAtt *findXMLAtt (XMLEle *e, const char *name);

/** \brief Find an XML element within an XML element.
    \param e a pointer to the XML element to search.
    \param tag the element tag to search for.
    \return A pointer to the XML element if found or NULL on failure.
*/
extern XMLEle *findXMLEle (XMLEle *e, const char *tag);

/* iteration functions */
/** \brief Iterate an XML element for a list of nesetd XML elements.
    \param ep a pointer to the XML element to iterate.
    \param first the index of the starting XML element. Pass 1 to start iteration from the beginning of the XML element. Pass 0 to get the next element thereater. 
    \return On success, a pointer to the next XML element is returned. NULL when there are no more elements.
*/
extern XMLEle *nextXMLEle (XMLEle *ep, int first);

/** \brief Iterate an XML element for a list of XML attributes.
    \param ep a pointer to the XML element to iterate.
    \param first the index of the starting XML attribute. Pass 1 to start iteration from the beginning of the XML element. Pass 0 to get the next attribute thereater. 
    \return On success, a pointer to the next XML attribute is returned. NULL when there are no more attributes.
*/
extern XMLAtt *nextXMLAtt (XMLEle *ep, int first);

/* tree functions */
/** \brief Return the parent of an XML element.
    \return a pointer to the XML element parent.
*/
extern XMLEle *parentXMLEle (XMLEle *ep);

/** \brief Return the parent of an XML attribute.
    \return a pointer to the XML element parent.
*/
extern XMLEle *parentXMLAtt (XMLAtt *ap);

/* access functions */
/** \brief Return the tag of an XML element.
    \param ep a pointer to an XML element.
    \return the tag string.
*/
extern char *tagXMLEle (XMLEle *ep);

/** \brief Return the pcdata of an XML element.
    \param ep a pointer to an XML element.
    \return the pcdata string on success.
*/
extern char *pcdataXMLEle (XMLEle *ep);

/** \brief Return the name of an XML attribute.
    \param ap a pointer to an XML attribute.
    \return the name string of the attribute.
*/
extern char *nameXMLAtt (XMLAtt *ap);

/** \brief Return the value of an XML attribute.
    \param ap a pointer to an XML attribute.
    \return the value string of the attribute.
*/
extern char *valuXMLAtt (XMLAtt *ap);

/** \brief Return the number of characters in pcdata in an XML element.
    \param ep a pointer to an XML element.
    \return the length of the pcdata string.
*/
extern int pcdatalenXMLEle (XMLEle *ep);

/** \brief Return the number of nested XML elements in a parent XML element.
    \param ep a pointer to an XML element.
    \return the number of nested XML elements.
*/
extern int nXMLEle (XMLEle *ep);

/** \brief Return the number of XML attributes in a parent XML element.
    \param ep a pointer to an XML element.
    \return the number of XML attributes within the XML element.
*/
extern int nXMLAtt (XMLEle *ep);

/* convenience functions */
/** \brief Find an XML element's attribute value.
    \param ep a pointer to an XML element.
    \param name the name of the XML attribute to retrieve its value.
    \return the value string of an XML element on success. NULL on failure.
*/
extern const char *findXMLAttValu (XMLEle *ep, char *name);

/** \brief Handy wrapper to read one xml file.
    \param fp pointer to FILE to read.
    \param lp pointer to lilxml parser.
    \param errmsg a buffer to store error messages on failure.
    \return root element else NULL with report in errmsg[].
*/
extern XMLEle *readXMLFile (FILE *fp, LilXML *lp, char errmsg[]);

/** \brief Print an XML element.
    \param fp a pointer to FILE where the print output is directed.
    \param e the XML element to print.
    \param level the printing level, set to 0 to print the whole element.
*/
extern void prXMLEle (FILE *fp, XMLEle *e, int level);

/*@}*/

#ifdef __cplusplus
}
#endif

/* examples.

        initialize a lil xml context and read an XML file in a root element

	LilXML *lp = newLilXML();
	char errmsg[1024];
	XMLEle *root, *ep;
	int c;

	while ((c = fgetc(stdin)) != EOF) {
	    root = readXMLEle (lp, c, errmsg);
	    if (root)
		break;
	    if (errmsg[0])
		error ("Error: %s\n", errmsg);
	}
 
        print the tag and pcdata content of each child element within the root

        for (ep = nextXMLEle (root, 1); ep != NULL; ep = nextXMLEle (root, 0))
	    printf ("%s: %s\n", tagXMLEle(ep), pcdataXMLEle(ep));


	finished with root element and with lil xml context

	delXMLEle (root);
	delLilXML (lp);
 */

/* For RCS Only -- Do Not Edit
 * @(#) $RCSfile$ $Date$ $Revision$ $Name$
 */

#endif	/* LILXML_H */
