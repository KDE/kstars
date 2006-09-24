#if 0
    INDI
    Copyright (C) 2006 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "indiapi.h"
#include "indidevapi.h"
#include "observer.h"
#include "indicom.h"

typedef struct 
{
    int in_use;
    char dev[MAXINDIDEVICE];
    char name[MAXINDINAME];
    IDType type;
    CBSP *fp;
} SP;

static SP *sp_sub;			/* malloced list of work procedures */
static int nsp_sub;			/* n entries in wproc[] */

typedef struct 
{
    int in_use;
    char dev[MAXINDIDEVICE];
    char name[MAXINDINAME];
    IPType property_type;
    IDType notification_type;
    fpt fp;
} OBP;

static OBP *oblist;			/* malloced list of work procedures */
static int noblist;			/* n entries in wproc[] */

void IOSubscribeSwitch(const char *dev, const char *name, IDType type, CBSP *fp)
{
        SP *sp;

	/* reuse first unused slot or grow */
	for (sp = sp_sub; sp < &sp_sub[nsp_sub]; sp++)
	    if (!sp->in_use)
		break;
	if (sp == &sp_sub[nsp_sub])
       {
	    sp_sub = sp_sub ? (SP *) realloc (sp_sub, (nsp_sub+1)*sizeof(SP))
	    		  : (SP *) malloc (sizeof(SP));
	    sp = &sp_sub[nsp_sub++];
	}

	/* init new entry */
	sp->in_use = 1;
	sp->fp = fp;
        sp->type = type;
	strncpy(sp->dev, dev, MAXINDIDEVICE);
	strncpy(sp->name, name, MAXINDINAME);
	

	printf ("<propertyVectorSubscribtion\n");
	printf ("  device='%s'\n", dev);
	printf ("  name='%s'\n", name);
	printf ("  action='subscribe'\n");
	printf ("  notification='%s'\n", idtypeStr(type));
	printf ("</propertyVectorSubscribtion>\n");
	fflush (stdout);

}

void IOUnsubscribeSwitch(const char *dev, const char *name)
{
   SP *sp;
	
    for (sp = sp_sub; sp < &sp_sub[nsp_sub]; sp++)
   {
       if (!strcmp(sp->dev, dev) && !strcmp(sp->name, name))
	{
		sp->in_use = 0;
		printf ("<propertyVectorSubscribtion\n");
		printf ("  device='%s'\n", dev);
		printf ("  name='%s'\n", name);
		printf ("  action='unsubscribe'\n");
		printf ("</propertyVectorSubscribtion>\n");
		fflush (stdout);
	}
   }
}

void IOSubscribeProperty(const char *dev, const char *name, IPType property_type, IDType notification_type, fpt fp)
{

        OBP *obp;

	/* reuse first unused slot or grow */
	for (obp = oblist; obp < &oblist[noblist]; obp++)
	    if (!obp->in_use)
		break;
	if (obp == &oblist[noblist])
       {
	    oblist = oblist ? (OBP *) realloc (oblist, (noblist+1)*sizeof(OBP))
	    		  : (OBP *) malloc (sizeof(OBP));
	    obp = &oblist[noblist++];
	}

	/* init new entry */
	obp->in_use = 1;
	obp->fp = fp;
	obp->property_type	= property_type;
        obp->notification_type	= notification_type;
	strncpy(obp->dev, dev, MAXINDIDEVICE);
	strncpy(obp->name, name, MAXINDINAME);
	

	printf ("<propertyVectorSubscribtion\n");
	printf ("  device='%s'\n", dev);
	printf ("  name='%s'\n", name);
	printf ("  action='subscribe'\n");
	printf ("  notification='%s' />", idtypeStr(notification_type));
	fflush (stdout);

}

void IOUnsubscribeProperty(const char *dev, const char *name)
{
    OBP *obp;
	
    for (obp = oblist; obp < &oblist[noblist]; obp++)
   {
       if (!strcmp(obp->dev, dev) && !strcmp(obp->name, name))
	{
		obp->in_use = 0;
		printf ("<propertyVectorSubscribtion\n");
		printf ("  device='%s'\n", dev);
		printf ("  name='%s'\n", name);
		printf ("  action='unsubscribe' />\n");
		fflush (stdout);
	}
   }

}

const char * idtypeStr(IDType type)
{
   switch (type)
  {
     case IDT_VALUE:
       	     return "Value";
	     break;

    case IDT_STATE:
	    return "State";
	    break;

    case IDT_ALL:
	    return "All";
	    break;
		  
    default:
	    return "Unknown";
	    break;
 }
}

int processObservers(XMLEle *root)
{
	OBP *obp;
	XMLAtt* ap;
        IDState state;
	char prop_dev[MAXINDIDEVICE];
	char prop_name[MAXINDINAME];
	   

	/* Driver sent which message? */
	if (strstr(tagXMLEle(root), "def"))
		state = IDS_DEFINED;
	else if (strstr(tagXMLEle(root), "set"))
		state = IDS_UPDATED;
	else if (strstr(tagXMLEle(root), "del"))
		state = IDS_DELETED;
	else
	{
		/* Silently ignore */
		return (-1);
	}
		
	ap = findXMLAtt(root, "device");
	if (!ap)
	{
		fprintf(stderr, "<%s> missing 'device' attribute.\n", tagXMLEle(root));
		exit(1);
	}
	else
		strncpy(prop_dev, valuXMLAtt(ap), MAXINDIDEVICE);
	
	/* Del prop might not have name, so don't panic */
	ap = findXMLAtt(root, "name");
	if (!ap && state != IDS_DELETED)
	{
		fprintf(stderr, "<%s> missing 'name' attribute.\n", tagXMLEle(root));
		exit(1);
	}
	else if (ap)
		strncpy(prop_name, valuXMLAtt(ap), MAXINDINAME);


    for (obp = oblist; obp < &oblist[noblist]; obp++)
   {
	/* We got a match */
	if (!strcmp(obp->dev, prop_dev) && ((state == IDS_DELETED) || (!strcmp(obp->name, prop_name))))
	{
	XMLEle *epx;
	int n;

	if (state == IDS_DELETED)
	{
		switch (obp->property_type)
		{
			case IPT_SWITCH:
			case IPT_TEXT:
			case IPT_NUMBER:
			case IPT_LIGHT:
				obp->fp(obp->dev, obp->name, IDS_DELETED, NULL, NULL, 0);
				break;
			case IPT_BLOB:
				obp->fp(obp->dev, obp->name, IDS_DELETED, NULL, NULL, NULL, NULL, 0);
				break;
		}
		continue;
	}

	/* check tag in surmised decreasing order of likelyhood */
	if (!strcmp (tagXMLEle(root), "setNumberVector") || !strcmp (tagXMLEle(root), "defNumberVector"))
	{
	    static double *doubles;
	    static char **names;
	    static int maxn;

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
			    IDLog ("%s: Bad format %s", obp->name,  pcdataXMLEle(epx));
			else
			    names[n++] = valuXMLAtt(na);
		    }
		}
	    }
	    
	    /* invoke driver if something to do, but not an error if not */
	    if (n > 0)
		obp->fp(obp->dev, obp->name, state, doubles, names, n);
	    else
		IDLog("%s: NumberVector with no valid members", obp->name);

	    return (0);
	}

	if (!strcmp (tagXMLEle(root), "setSwitchVector") || !strcmp (tagXMLEle(root), "defSwitchVector")) {
	    static ISState *states;
	    static char **names;
	    static int maxn;

	    /* seed for reallocs */
	    if (!states) {
		states = (ISState *) malloc (sizeof(void*));
		names = (char **) malloc (sizeof(void*));
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
			    IDLog ("%s: must be On or Off: %s\n",  obp->name,    pcdataXMLEle(epx));
		    }
		}
	    }

	    /* invoke driver if something to do, but not an error if not */
	    if (n > 0)
		obp->fp(obp->dev, obp->name, state, states, names, n);
	    else
		IDLog("%s: SwitchVector with no valid members", obp->name);

	    return (0);
	}

	if (!strcmp (tagXMLEle(root), "setTextVector") || !strcmp (tagXMLEle(root), "defTextVector")) {
	    static char **texts;
	    static char **names;
	    static int maxn;

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
	    
	    /* invoke driver if something to do, but not an error if not */
	    if (n > 0)
		obp->fp(obp->dev, obp->name, state, texts, names, n);
	    else
		IDLog("%s: TextVector with no valid members", obp->name);

	    return (0);
	}

	if (!strcmp (tagXMLEle(root), "setBLOBVector") || !strcmp (tagXMLEle(root), "defBLOBVector")) {
	    static char **blobs;
	    static char **names;
	    static char **formats;
	    static int *blobsizes;
	    static int maxn;

	    /* seed for reallocs */
	    if (!blobs) {
		blobs = (char **) malloc (sizeof(void*));
		names = (char **) malloc (sizeof(void*));
		formats = (char **) malloc (sizeof(void*));
		blobsizes = (int *) malloc (sizeof(void*));
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
			    blobsizes = (int *) realloc(blobsizes,newsz);
			}
			blobs[n] = pcdataXMLEle(epx);
			names[n] = valuXMLAtt(na);
			formats[n] = valuXMLAtt(fa);
			blobsizes[n] = atoi(valuXMLAtt(sa));
			n++;
		    }
		}
	    }

	    /* invoke driver if something to do, but not an error if not */
	    if (n > 0)
		obp->fp(obp->dev, obp->name, state, blobsizes, blobs, formats, names, n);
	    else
		IDLog("%s: BLOBVector with no valid members", obp->name);

	    return (0);
	}
	else return (-1);

	/* FIXME TODO need to fetch LIGHT!!! */

	} /* End if */

   } /* End For */

  return (0);
	
}

int crackObserverState(char *stateStr)
{
	if (!strcmp(stateStr, "Value"))
		return (IDT_VALUE);
	else if (!strcmp(stateStr, "State"))
		return (IDT_STATE);
	else if (!strcmp(stateStr, "All"))
		return (IDT_ALL);
	
	else return -1;
}

int crackPropertyState(char *pstateStr)
{
	if (!strcmp(pstateStr, "Idle"))
		return IPS_IDLE;
	else if (!strcmp(pstateStr, "Ok"))
		return IPS_OK;
	else if (!strcmp(pstateStr, "Busy"))
		return IPS_BUSY;
	else if (!strcmp(pstateStr, "Alert"))
		return IPS_ALERT;
	else return -1;
}


