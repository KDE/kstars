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

/* simulate a telescope mount on an INDI interface.
 * it can be told to slew to an RA or Dec and will go there slowly enough to
 * watch.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
//#include <math.h>
#include <sys/time.h>
#include <unistd.h>

#include "indiapi.h"

static void sendeq (char *msg);
static void sendNewPow (void);
static void mountSim (void *);

#define	SLEWRATE	1			/* slew rate, degrees/s */
#define	POLLMS		250			/* poll period, ms */
#define SIDRATE		0.004178		/* sidereal rate, degrees/s */


/* INDI controls */

static char mydev[] = "Telescope Simulator";	/* our Device name */

/* equatorial position */
static INumber eq[] = {
    {"RA",  "RA  H:M:S", "%10.6m",  0., 24., 0., 0.},
    {"DEC", "Dec D:M:S", "%10.6m", -90., 90., 0., 0.},
};
static INumberVectorProperty eqNum = {
    mydev, "EQUATORIAL_COORD", "Equatorial J2000", NULL, IP_RW, 0, IPS_IDLE,
    eq, NARRAY(eq),
};

/* main power switch */
static ISwitch power[] = {
    {"CONNECT",  "On",  ISS_OFF},
};
static ISwitchVectorProperty powSw = {
    mydev, "CONNECTION", "Connection", NULL, IP_RW, ISR_1OFMANY, 0., IPS_IDLE,
    power, NARRAY(power),
};

/* operational info */
static double targetRA;
static double targetDEC;
static double currentRA;	/* scope's current simulated RA, rads */
static double currentDec;	/* scope's current simulated Dec, rads */

/* call first, then benign there after */
static void
mountInit()
{
	static int inited;		/* set once mountInit is called */

	if (inited)
	    return;
	
	/* start timer to simulate mount motion */
	IEAddTimer (POLLMS, mountSim, NULL);

	inited = 1;
	
}

/* send client definitions of all properties */
void
ISGetProperties (const char *dev)
{
	if (dev && strcmp (mydev, dev))
	    return;

	IDDefNumber (&eqNum, NULL);
	IDDefSwitch (&powSw, NULL);
}

void ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{}

/* client is sending us a new value for a Numeric vector property */
void
ISNewNumber (const char *dev, const char *name, double values[],
char *names[], int n)
{
	char msg[64];

	/* ignore if not ours */
	if (strcmp (dev, mydev))
	    return;

	mountInit();

	if (!strcmp (name, eqNum.name)) {
	    /* new equatorial target coords */
	    double newra = 0, newdec = 0;
	    int i, nset;
	    if (power[0].s != ISS_ON) {
		eqNum.s = IPS_IDLE;
		sendeq ("Power is off");
		return;
	    }
	    for (nset = i = 0; i < n; i++) {
		INumber *eqp = IUFindNumber (&eqNum, names[i]);
		if (eqp == &eq[0]) {
		    newra = (values[i]);
		    nset += newra >= 0 && newra <= 24;
		} else if (eqp == &eq[1]) {
		    newdec = (values[i]);
		    nset += newdec >= -90 && newdec <= 90;
		}
	    }
	    if (nset == 2) {
		char r[32], d[32];
		eqNum.s = IPS_BUSY;
		targetRA = newra;
		targetDEC = newdec;
		fs_sexa (r, targetRA, 2, 3600);
		fs_sexa (d, targetDEC, 3, 3600);
		snprintf (msg, sizeof(msg), "Moving to RA Dec %.32s %.32s", r, d);
	    } else {
		eqNum.s = IPS_IDLE;
		snprintf (msg, sizeof(msg), "RA or Dec absent or bogus");
	    }
	    sendeq (msg);
	    return;
	}
}

/* client is sending us a new value for a Switch property */
void
ISNewSwitch (const char *dev, const char *name, ISState *states,
char *names[], int n)
{
	ISwitch *sp;

	/* ignore if not ours */
	if (strcmp (dev, mydev))
	    return;

	IDLog("Before mount init \n");
	mountInit();

	IDLog("before find switch\n");
	sp = IUFindSwitch (&powSw, names[0]);
	
	IDLog("after find switch\n");
	
	if (sp) {
	    sp->s = states[0];
	    sendNewPow ();
	}
	
	IDLog("after send now\n");
	/*powSw.sw[0].s = states[0];*/
}

/* update the "mount" over time */
void
mountSim (void *p)
{
	static struct timeval ltv;
	struct timeval tv;
	double dt, da, dx;
	int nlocked;

	/* update elapsed time since last poll, don't presume exactly POLLMS */
	gettimeofday (&tv, NULL);
	if (ltv.tv_sec == 0 && ltv.tv_usec == 0)
	    ltv = tv;
	dt = tv.tv_sec - ltv.tv_sec + (tv.tv_usec - ltv.tv_usec)/1e6;
	ltv = tv;
	da = SLEWRATE*dt;

	/* process per current state */
	switch (eqNum.s)
	{
	case IPS_IDLE:
	    /* RA moves at sidereal, Dec stands still */
	    currentRA += (SIDRATE*dt/15.);
	    sendeq (NULL);
	    break;

	case IPS_BUSY:
	    /* slewing - nail it when both within one pulse @ SLEWRATE */
	    nlocked = 0;

	    dx = targetRA - currentRA;
	    if (fabs(dx) <= da)
	    {
		currentRA = targetRA;
		nlocked++;
	    }
	    else if (dx > 0)
	    	currentRA += da/15.;
	    else 
	    	currentRA -= da/15.;
	    

	    dx = targetDEC - currentDec;
	    if (fabs(dx) <= da)
	    {
		currentDec = targetDEC;
		nlocked++;
	    }
	    else if (dx > 0)
	      currentDec += da;
	    else
	      currentDec -= da;

	    if (nlocked == 2)
	    {
		eqNum.s = IPS_OK;
		sendeq("Now tracking");
	    } else
		sendeq(NULL);

	    break;

	case IPS_OK:
	    /* tracking */
	    sendeq (NULL);
	    break;

	case IPS_ALERT:
	    break;
	}

	/* again */
	IEAddTimer (POLLMS, mountSim, NULL);
}

/* set eqNum from currentRA and Dec and send to client with optional message */
static void
sendeq (char *msg)
{
	eq[0].value = currentRA;
	eq[1].value = currentDec;
	IDSetNumber (&eqNum, msg);
}

static void
sendNewPow ()
{
	if (power[0].s == ISS_ON) {
	    powSw.s = IPS_OK;
	    IDSetSwitch (&powSw, "Power on");
	    eqNum.s = IPS_OK;
	} else {
	    powSw.s = IPS_IDLE;
	    IDSetSwitch (&powSw, "Power off");
	    eqNum.s = IPS_IDLE;
	    sendeq (NULL);
	}
}
