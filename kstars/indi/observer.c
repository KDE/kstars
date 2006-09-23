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
#include "observer.h"

typedef struct 
{
    int in_use;
    char dev[MAXINDIDEVICE];
    char name[MAXINDINAME];
     IDType data_type;
    CBSP *fp;
} SP;

static SP *sp_sub;			/* malloced list of work procedures */
static int nsp_sub;			/* n entries in wproc[] */

void IOSubscribeSwitch(const char *dev, const char *name, IDType data_type, CBSP *fp)
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
        sp->data_type = data_type;
	strncpy(sp->dev, dev, MAXINDIDEVICE);
	strncpy(sp->name, name, MAXINDINAME);
	

	printf ("<switchVectorSubscribtion\n");
	printf ("  device='%s'\n", dev);
	printf ("  name='%s'\n", name);
	printf (" action='subscribe'\n");
	printf ("  notification='%s'\n", idtypeStr(data_type));
	printf ("</switchVectorSubscribtion>\n");
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
		printf ("<switchVectorSubscribtion\n");
		printf ("  device='%s'\n", dev);
		printf ("  name='%s'\n", name);
		printf (" action='unsubscribe'\n");
		printf ("</switchVectorSubscribtion>\n");
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

    default:
		return "Unknown";
		break;
 }
}



