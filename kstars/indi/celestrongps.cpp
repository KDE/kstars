#if 0
    Celestron GPS
    Copyright (C) 2003 Jasem Mutlaq (mutlaqja@ikarustech.com)

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
#include <unistd.h>
#include <time.h>

#include "celestronprotocol.h"
#include "celestrongps.h"

#define RA_THRESHOLD	0.01
#define DEC_THRESHOLD	0.05

CelestronGPS *telescope = NULL;
char mydev[] = "Celestron GPS";

/* There is _one_ binary for all LX200 drivers, but each binary is renamed
** to its device name (i.e. lx200gps, lx200_16..etc). The main function will
** fetch from std args the binary name and ISInit will create the apporpiate
** device afterwards. If the binary name does not match any known devices,
** we simply create a generic device
*/
extern char* me;

#define COMM_GROUP	"Communication"
#define BASIC_GROUP	"Basic Data"
#define MOVE_GROUP	"Movement Control"

static void ISPoll(void *);

/*INDI controls */
static ISwitch PowerS[]          = {{"CONNECT" , "Connect" , ISS_OFF},{"DISCONNECT", "Disconnect", ISS_ON}};
static ISwitch SlewModeS[]       = {{"Slew", "", ISS_ON}, {"Find", "", ISS_OFF}, {"Centering", "", ISS_OFF}, {"Guide", "", ISS_OFF}};
static ISwitch OnCoordSetS[]     = {{"Slew", "", ISS_ON }, {"Track", "", ISS_OFF}, {"Sync", "", ISS_OFF }};
static ISwitch abortSlewS[]      = {{"Abort", "Abort Slew/Track", ISS_OFF }};

static ISwitch MovementS[]       = {{"N", "North", ISS_OFF}, {"W", "West", ISS_OFF}, {"E", "East", ISS_OFF}, {"S", "South", ISS_OFF}};
static ISwitch haltMoveS[]       = {{"TN", "Northward", ISS_OFF}, {"TW", "Westward", ISS_OFF}, {"TE", "Eastward", ISS_OFF}, {"TS", "Southward", ISS_OFF}};

/* equatorial position */
static INumber eq[] = {
    {"RA",  "RA  H:M:S", "%10.6m",  0., 24., 0., 0.},
    {"DEC", "Dec D:M:S", "%10.6m", -90., 90., 0., 0.},
};
//TODO decide appropiate TIME_OUT
static INumberVectorProperty eqNum = {
    mydev, "EQUATORIAL_COORD", "Equatorial J2000", BASIC_GROUP, IP_RW, 0, IPS_IDLE,
    eq, NARRAY(eq),
};

/* Fundamental group */
static ISwitchVectorProperty PowerSw	= { mydev, "CONNECTION" , "Connection", COMM_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, PowerS, NARRAY(PowerS)};
static IText PortT[]			= {{"Port", "Port", "/dev/ttyS0"}};
static ITextVectorProperty Port		= { mydev, "Ports", "Ports", COMM_GROUP, IP_RW, 0, IPS_IDLE, PortT, NARRAY(PortT)};

/* Movement group */
static ISwitchVectorProperty OnCoordSetSw    = { mydev, "ON_COORD_SET", "On Set", BASIC_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, OnCoordSetS, NARRAY(OnCoordSetS)};
static ISwitchVectorProperty abortSlewSw     = { mydev, "ABORT_MOTION", "Abort", BASIC_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, abortSlewS, NARRAY(abortSlewS)};
static ISwitchVectorProperty SlewModeSw      = { mydev, "Slew rate", "", MOVE_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, SlewModeS, NARRAY(SlewModeS)};

static ISwitchVectorProperty MovementSw      = { mydev, "Move toward", "", MOVE_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, MovementS, NARRAY(MovementS)};
static ISwitchVectorProperty haltMoveSw      = { mydev, "Halt movement", "", MOVE_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, haltMoveS, NARRAY(haltMoveS)};

/* send client definitions of all properties */
void ISInit()
{
  static int isInit=0;

 if (isInit)
  return;

 isInit = 1;

  telescope = new CelestronGPS();

   IEAddTimer (POLLMS, ISPoll, NULL);
}

void ISGetProperties (const char *dev)
{ ISInit(); telescope->ISGetProperties(dev);}
void ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{ ISInit(); telescope->ISNewSwitch(dev, name, states, names, n);}
void ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{ ISInit(); telescope->ISNewText(dev, name, texts, names, n);}
void ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{ ISInit(); telescope->ISNewNumber(dev, name, values, names, n);}
void ISPoll (void *p) { telescope->ISPoll(); IEAddTimer (POLLMS, ISPoll, NULL); p=p;}

/**************************************************
*** LX200 Generic Implementation
***************************************************/

CelestronGPS::CelestronGPS()
{

   targetRA  = lastRA = 0;
   targetDEC = lastDEC = 0;
   lastSet   = 0;
   lastMove[0] = lastMove[1] = lastMove[2] = lastMove[3] = 0;

   JD = 0;

   // Children call parent routines, this is the default
   IDLog("initilizaing from Celeston GPS device...\n");

}

void CelestronGPS::ISGetProperties(const char *dev)
{

 if (dev && strcmp (mydev, dev))
    return;

  // COMM_GROUP
  IDDefSwitch (&PowerSw);
  IDDefText   (&Port);


  // BASIC_GROUP
  IDDefNumber (&eqNum);
  IDDefSwitch (&OnCoordSetSw);
  IDDefSwitch (&abortSlewSw);
  IDDefSwitch (&SlewModeSw);

  // Movement group
  IDDefSwitch (&MovementSw);
  IDDefSwitch (&haltMoveSw);

  //TODO this is really a test message only
  IDMessage(NULL, "KTelescope components registered successfully");
}

void CelestronGPS::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
        IText *tp;

	// suppress warning
	n=n;
	// ignore if not ours //
	if (strcmp (dev, mydev))
	    return;

	if (!strcmp(name, Port.name) )
	{
	  Port.s = IPS_OK;

	  tp = IUFindText( &Port, names[0] );
	  if (!tp)
	   return;

	  tp->text = new char[strlen(texts[0])+1];
	  strcpy(tp->text, texts[0]);
	  IDSetText (&Port, NULL);
	  return;
	}
}

int CelestronGPS::handleCoordSet()
{

  int i=0;
  char RAStr[32], DecStr[32];

  switch (lastSet)
  {

    // Slew & Track
    case 0:
    case 1:

	  if (OnCoordSetSw.s == IPS_BUSY)
	  {
	     StopNSEW();

	     // sleep for 500 mseconds
	     usleep(500000);
	  }

	  if ((i = SlewToCoords(targetRA, targetDEC)))
	  {
	    slewError(i);
	    return (-1);
	  }

	  OnCoordSetSw.s = IPS_BUSY;
	  eqNum.s = IPS_BUSY;
	  fs_sexa(RAStr, eqNum.n[0].value, 2, 3600);
	  fs_sexa(DecStr, eqNum.n[1].value, 2, 3600);
	  IDSetSwitch(&OnCoordSetSw, "Slewing to J2000 RA %s - DEC %s", RAStr, DecStr);
	  IDSetNumber(&eqNum, NULL);
	  break;


  /*// track
  case 1:

         abortSlew();

	 // sleep for 200 mseconds
	 usleep(200000);


          if ((i = Slew()))
	  {
	    slewError(i);
	    return (-1);
	  }

	  OnCoordSetSw.s = IPS_BUSY;
	  fs_sexa(RAStr, targetRA, 2, 3600);
	  fs_sexa(DecStr, targetDEC, 2, 3600);
	  IDSetSwitch(&OnCoordSetSw, "Slewing to J2000 RA %s - DEC %s", RAStr, DecStr);
	  IDSetNumber(&eqNum, NULL);

	  //IDSetSwitch(&OnCoordSetSw, "Tracking...");
   break;
*/
  // Sync
  case 2:
	  OnCoordSetSw.s = IPS_OK;
	  SyncToCoords(targetRA, targetDEC);

          resetSwitches(&OnCoordSetSw);
          OnCoordSetSw.sw[2].s = ISS_ON;
          eqNum.s = IPS_OK;
   	  IDSetNumber(&eqNum, NULL);
	  IDSetSwitch(&OnCoordSetSw, "Synchronization successful.");
	  break;
   }

   return (0);

}

void CelestronGPS::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
        double newRA=0, newDEC=0;

        // ignore if not ours //
	if (strcmp (dev, mydev))
	    return;

	struct tm *tp;
	time_t t;

	time (&t);
	tp = gmtime (&t);

	if (!strcmp (name, eqNum.name))
	{
	  IDLog("in EQ number\n");
	  int i=0, nset=0;

	  if (checkPower(&eqNum))
	   return;

	    for (nset = i = 0; i < n; i++)
	    {
		INumber *eqp = IUFindNumber (&eqNum, names[i]);
		if (eqp == &eq[0])
		{
                    newRA = values[i];
		    nset += newRA >= 0 && newRA <= 24.0;
		} else if (eqp == &eq[1])
		{
		    newDEC = values[i];
		    nset += newDEC >= -90.0 && newDEC <= 90.0;
		}
	    }

	  if (nset == 2)
	  {
	   eqNum.s = IPS_BUSY;

	   tp->tm_mon   += 1;
	   tp->tm_year  += 1900;

	   // update JD
           JD = UTtoJD(tp);

	   IDLog("We recevined J2000 RA %f - DEC %f\n", newRA, newDEC);;
	   apparentCoord( (double) J2000, JD, &newRA, &newDEC);
	   IDLog("Processed to RA %f - DEC %f\n", newRA, newDEC);

	       eqNum.n[0].value = values[0];
	       eqNum.n[1].value = values[1];
	       targetRA  = newRA;
	       targetDEC = newDEC;

	       if (handleCoordSet())
	       {
	        eqNum.s = IPS_IDLE;
	    	IDSetNumber(&eqNum, NULL);
	       }
	    }
	    else
	    {
		eqNum.s = IPS_IDLE;
		IDSetNumber(&eqNum, "RA or Dec missing or invalid");
	    }

	    return;
	 }
}

void CelestronGPS::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{

        int index;

	// Suppress warning
	names = names;

	IDLog("in new Switch with Device= %s and Property= %s and #%d items\n", dev, name,n);
	//IDLog("SolarSw name is %s\n", SolarSw.name);

	// ignore if not ours //
	if (strcmp (dev, mydev))
	    return;

	// FIRST Switch ALWAYS for power
	if (!strcmp (name, PowerSw.name))
	{
   	 powerTelescope(states);
	 return;
	}

	if (!strcmp(name, OnCoordSetSw.name))
	{
  	  if (checkPower(&OnCoordSetSw))
	   return;

	  lastSet = getOnSwitch(states, n);
	  handleCoordSet();
	}

	// Abort Slew
	if (!strcmp (name, abortSlewSw.name))
	{
	  if (checkPower(&abortSlewSw))
	  {
	    abortSlewSw.s = IPS_IDLE;
	    IDSetSwitch(&abortSlewSw, NULL);
	    return;
	  }

	    if (OnCoordSetSw.s == IPS_BUSY || OnCoordSetSw.s == IPS_OK)
	    {
	    	StopNSEW();
		abortSlewSw.s = IPS_OK;
		abortSlewSw.sw[0].s = ISS_OFF;
		OnCoordSetSw.s = IPS_IDLE;
		eqNum.s = IPS_IDLE;
		IDSetSwitch(&abortSlewSw, "Slew aborted");
		IDSetSwitch(&OnCoordSetSw, NULL);
		IDSetNumber(&eqNum, NULL);

            }
	    else if (MovementSw.s == IPS_BUSY)
	    {
	        StopSlew(NORTH);
		StopSlew(WEST);
		StopSlew(EAST);
		StopSlew(SOUTH);
		lastMove[0] = lastMove[1] = lastMove[2] = lastMove[3] = 0;
		abortSlewSw.s = IPS_OK;
		abortSlewSw.sw[0].s = ISS_OFF;
		MovementSw.s = IPS_IDLE;
		resetSwitches(&MovementSw);
		eqNum.s = IPS_IDLE;
		IDSetSwitch(&abortSlewSw, "Slew aborted");
		IDSetSwitch(&MovementSw, NULL);
		IDSetNumber(&eqNum, NULL);
	    }
	    else
	    {
	        abortSlewSw.sw[0].s = ISS_OFF;
	        abortSlewSw.s = IPS_IDLE;
	        IDSetSwitch(&abortSlewSw, NULL);
	    }

	    return;
	}


	// Slew mode
	if (!strcmp (name, SlewModeSw.name))
	{
	  if (checkPower(&SlewModeSw))
	   return;

	  index = getOnSwitch(states, n);
	  SetRate(index);
          resetSwitches(&SlewModeSw);
	  SlewModeSw.sw[index].s = ISS_ON;

	  SlewModeSw.s = IPS_OK;
	  IDSetSwitch(&SlewModeSw, NULL);
	  return;
	}

	if (!strcmp (name, MovementSw.name))
	{
	  if (checkPower(&MovementSw))
	   return;

	 index = getOnSwitch(states, n);

	 if (index < 0)
	  return;

	  if (lastMove[index])
	    return;

	  lastMove[index] = 1;

	  StartSlew(index);

	  for (uint i=0; i < 4; i++)
	    MovementSw.sw[i].s = lastMove[i] == 0 ? ISS_OFF : ISS_ON;

	  MovementSw.s = IPS_BUSY;
	  IDSetSwitch(&MovementSw, "Moving %s...", Direction[index]);
	  return;
	}

	// Halt Movement
	if (!strcmp (name, haltMoveSw.name))
	{
	  if (checkPower(&haltMoveSw))
	   return;

	  index = getOnSwitch(states, n);

	  if (MovementSw.s == IPS_BUSY)
	  {
	  	StopSlew(index);
		lastMove[index] = 0;

		if (!lastMove[0] && !lastMove[1] && !lastMove[2] && !lastMove[3])
		  MovementSw.s = IPS_IDLE;

		for (uint i=0; i < 4; i++)
		{
	    	   haltMoveSw.sw[i].s = ISS_OFF;
		   MovementSw.sw[i].s = lastMove[i] == 0 ? ISS_OFF : ISS_ON;
		}

		eqNum.s = IPS_IDLE;
		haltMoveSw.s = IPS_IDLE;

		IDSetSwitch(&haltMoveSw, "Moving toward %s aborted", Direction[index]);
	  	IDSetSwitch(&MovementSw, NULL);
	  }
	  else
	  {
	        haltMoveSw.sw[index].s = ISS_OFF;
	     	haltMoveSw.s = IPS_IDLE;
	        IDSetSwitch(&haltMoveSw, NULL);
	  }
	  return;
	 }

}

void CelestronGPS::resetSwitches(ISwitchVectorProperty *driverSw)
{

   for (int i=0; i < driverSw->nsw; i++)
      driverSw->sw[i].s = ISS_OFF;

}

int CelestronGPS::getOnSwitch(ISState * states, int n)
{
 for (int i=0; i < n ; i++)
     if (states[i] == ISS_ON)
      return i;

 return -1;
}


int CelestronGPS::checkPower(ISwitchVectorProperty *sp)
{
  if (PowerSw.s != IPS_OK)
  {
    IDMessage (mydev, "Cannot change a property while the telescope is offline.");
    sp->s = IPS_IDLE;
    IDSetSwitch(sp, NULL);
    return -1;
  }

  return 0;
}

int CelestronGPS::checkPower(INumberVectorProperty *np)
{

  if (PowerSw.s != IPS_OK)
  {
    IDMessage (mydev, "Cannot change a property while the telescope is offline");
    np->s = IPS_IDLE;
    IDSetNumber(np, NULL);
    return -1;
  }

  return 0;

}

int CelestronGPS::checkPower(ITextVectorProperty *tp)
{

  if (PowerSw.s != IPS_OK)
  {
    IDMessage (mydev, "Cannot change a property while the telescope is offline");
    tp->s = IPS_IDLE;
    IDSetText(tp, NULL);
    return -1;
  }

  return 0;

}

void CelestronGPS::ISPoll()
{

       double dx, dy;
       double currentRA, currentDEC;
       int status;


	switch (OnCoordSetSw.s)
	{
	case IPS_IDLE:
	if (PowerSw.s != IPS_OK)
	 break;
	currentRA = GetRA();
	currentDEC = GetDec();

        if ( fabs (currentRA - lastRA) > 0.01 || fabs (currentDEC - lastDEC) > 0.01)
	{
	        eqNum.n[0].value = currentRA;
		eqNum.n[1].value = currentDEC;
		lastRA  = currentRA;
		lastDEC = currentDEC;
		IDSetNumber (&eqNum, NULL);

	}
        break;

        case IPS_BUSY:
	IDLog("in POLL _ BUSY\n");

	    currentRA = GetRA();
	    currentDEC = GetDec();
	    dx = targetRA - currentRA;
	    dy = targetDEC - currentDEC;

	    IDLog("targetRA is %f, currentRA is %f\n", (float) targetRA, (float) currentRA);
	    IDLog("targetDEC is %f, currentDEC is %f\n****************************\n", (float) targetDEC, (float) currentDEC);

	    eqNum.n[0].value = currentRA;
	    eqNum.n[1].value = currentDEC;

	    status = CheckCoords(targetRA, targetDEC);

	    // Wait until acknowledged or within 3.6', change as desired.
	    switch (status)
	    {
	    case 0:		/* goto in progress */
		IDSetNumber (&eqNum, NULL);
		break;
	    case 1:		/* goto complete within tolerance */
	    case 2:		/* goto complete but outside tolerance */
		currentRA = targetRA;
		currentDEC = targetDEC;

		apparentCoord( JD, (double) J2000, &currentRA, &currentDEC);

		eqNum.n[0].value = currentRA;
		eqNum.n[1].value = currentDEC;



		//formatSex (targetRA, eq[0].nstr, XXYYZZ);
		//formatSex (targetDEC, DEC.nstr, SXXYYZZ);
		OnCoordSetSw.s = IPS_OK;

		if (lastSet == 0)
		{

		  //eq[0].s = IPS_OK;
		  //DEC.s = IPS_OK;
		  eqNum.s = IPS_OK;
		  resetSwitches(&OnCoordSetSw);
		  OnCoordSetSw.sw[0].s = ISS_ON;
		  IDSetSwitch (&OnCoordSetSw, "Slew is complete");
		}
		else
		{
		  eqNum.s = IPS_OK;
		  resetSwitches(&OnCoordSetSw);
		  OnCoordSetSw.sw[1].s = ISS_ON;
		  IDSetSwitch (&OnCoordSetSw, "Slew is complete. Tracking...");
		  //abortSlew();
		  //IDSetSwitch (&OnCoordSetSw, NULL);
		}

		 IDSetNumber (&eqNum, NULL);
		 //IDSetNumber (&DEC, NULL);
		break;
	    }   
	    break;

	case IPS_OK:
	if (PowerSw.s != IPS_OK)
	 break;
	currentRA = GetRA();
	currentDEC = GetDec();

        if ( fabs (currentRA - lastRA) > 0.01 || fabs (currentDEC - lastDEC) > 0.01)
	{

		eqNum.n[0].value = currentRA;
		eqNum.n[1].value = currentDEC;
		lastRA  = currentRA;
		lastDEC = currentDEC;
		IDSetNumber (&eqNum, NULL);

	}
        break;


	case IPS_ALERT:
	    break;
	}

	switch (MovementSw.s)
	{
	  case IPS_IDLE:
	   break;
	 case IPS_BUSY:
	     currentRA = GetRA();
	     currentDEC = GetDec();

	     apparentCoord( JD, (double) J2000, &currentRA, &currentDEC);


	     eqNum.n[0].value = currentRA;
	     eqNum.n[1].value = currentDEC;

	     IDSetNumber (&eqNum, NULL);

	     break;
	 case IPS_OK:
	   break;
	 case IPS_ALERT:
	   break;
	 }

}

void CelestronGPS::getBasicData()
{

  targetRA = GetRA();
  targetDEC = GetDec();

  eqNum.n[0].value = targetRA;
  eqNum.n[1].value = targetDEC;

  IDSetNumber(&eqNum, NULL);

}

void CelestronGPS::powerTelescope(ISState *s)
{

 for (uint i= 0; i < NARRAY(PowerS); i++)
     PowerS[i].s = s[i];

     switch (PowerSw.sw[0].s)
     {
      case ISS_ON:

         if (ConnectTel(Port.t[0].text) < 0)
	 {
	   PowerS[0].s = ISS_OFF;
	   PowerS[1].s = ISS_ON;
	   IDSetSwitch (&PowerSw, "Error connecting to port %s", Port.t[0].text);
	   return;
	 }

	PowerSw.s = IPS_OK;
	IDSetSwitch (&PowerSw, "Telescope is online. Retrieving basic data...");
	getBasicData();
	break;

     case ISS_OFF:
         IDSetSwitch (&PowerSw, "Telescope is offline.");
	 IDLog("Telescope is offline.");
	 DisconnectTel();
	 break;

    }
}

void CelestronGPS::slewError(int slewCode)
{
    OnCoordSetSw.s = IPS_IDLE;

    switch (slewCode)
    {
      case 1:
       IDSetSwitch (&OnCoordSetSw, "Invalid newDec in SlewToCoords");
       break;
      case 2:
       IDSetSwitch (&OnCoordSetSw, "RA count overflow in SlewToCoords");
       break;
      case 3:
       IDSetSwitch (&OnCoordSetSw, "Dec count overflow in SlewToCoords");
       break;
      case 4:
       IDSetSwitch (&OnCoordSetSw, "No acknowledgement from telescope after SlewToCoords");
       break;
      default:
       IDSetSwitch (&OnCoordSetSw, "Unknown error");
       break;
    }

}
