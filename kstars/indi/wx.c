#if 0
    Simulate a weather station

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <sys/time.h>
#include <unistd.h>

#include "indiapi.h"

static void sendWX (void);

#define	POLLMS		1000			/* poll period, ms */

/* operaional info */
static char mydev[] = "Weather Station Simulation";/* Device name we call ourselves */
static double t = 10;		/* temp, C */
static double p = 1002;		/* pressure, hPa */
static double d = 7;		/* dew point, C */
static double w = 345;		/* wind dir, deg E of N */
static double s = 22;		/* wind speed, kph */

/* INDI controls */
static INumber tNum = {mydev, "AmbTemp",  NULL, ILS_OK, 0};
static INumber pNum = {mydev, "AtmPres",  NULL, ILS_OK, 0};
static INumber dNum = {mydev, "DewPoint", NULL, ILS_OK, 0};
static INumber wNum = {mydev, "WindDir",  NULL, ILS_OK, 0};
static INumber sNum = {mydev, "WindSpd",  NULL, ILS_OK, 0};

void
ISInit()
{
	tNum.nstr = strcpy (malloc (10), "");
	pNum.nstr = strcpy (malloc (10), "");
	dNum.nstr = strcpy (malloc (10), "");
	wNum.nstr = strcpy (malloc (10), "");
	sNum.nstr = strcpy (malloc (10), "");

	ICPollMe (POLLMS);
}

/* send client definitions of all properties */
void
ISGetProperties (char *dev)
{
	if (dev && strcmp (mydev, dev))
	    return;

	ICDefNumber (&tNum,    "Temp, °C", IP_RO, NULL);
	ICDefNumber (&pNum,    "Pressue, hPa", IP_RO, NULL);
	ICDefNumber (&dNum,    "Dew point, °C", IP_RO, NULL);
	ICDefNumber (&wNum,    "Wind dir, °E of N", IP_RO, NULL);
	ICDefNumber (&sNum,    "Wind speed, kph", IP_RO, NULL);
}

void
ISNewNumber (INumber *n)
{
}

void
ISNewText (IText *t)
{
}

void
ISNewSwitch (ISwitches *s)
{
}

/* update the "weather" over time */
void
ISPoll ()
{
	t += .1 * (rand() - RAND_MAX/2.)/(RAND_MAX/2.);
	p += .1 * (rand() - RAND_MAX/2.)/(RAND_MAX/2.);
	d += .1 * (rand() - RAND_MAX/2.)/(RAND_MAX/2.);
	if (d > t)
	    d = t;
	w += 2. * (rand() - RAND_MAX/2.)/(RAND_MAX/2.);
	s += 1. * (rand() - RAND_MAX/2.)/(RAND_MAX/2.);
	if (s < 0)
	    s = 0;

	sendWX();
}

static void
sendWX ()
{
	char *msg;

	/* t */
	sprintf (tNum.nstr, "%.1f", t);
	ICSetNumber (&tNum, NULL);

	/* p */
	sprintf (pNum.nstr, "%.1f", p);
	ICSetNumber (&pNum, NULL);

	/* d */
	msg = NULL;
	if (d > t-3) {
	    if (dNum.s != ILS_ALERT) {
		msg = "Humidity approaching condensation levels";
		dNum.s = ILS_ALERT;
	    }
	} else {
	    if (dNum.s == ILS_ALERT) {
		msg = "Condensation threat is gone";
		dNum.s = ILS_OK;
	    }
	}
	sprintf (dNum.nstr, "%.1f", d);
	ICSetNumber (&dNum, msg);

	/* w */
	sprintf (wNum.nstr, "%.1f", w);
	ICSetNumber (&wNum, NULL);

	/* s */
	msg = NULL;
	if (s > 25) {
	    if (sNum.s != ILS_ALERT) {
		msg = "Weed speed now exceeds 25 kph";
		sNum.s = ILS_ALERT;
	    }
	} else {
	    if (sNum.s == ILS_ALERT) {
		msg = "High wind warning is gone";
		sNum.s = ILS_OK;
	    }
	}
	sprintf (sNum.nstr, "%.1f", s);
	ICSetNumber (&sNum, msg);
}
