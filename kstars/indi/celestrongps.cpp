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

CelestronGPS *telescope = NULL;
int MaxReticleFlashRate = 3;
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

void ISInit()
{
   telescope = new CelestronGPS();
}

static ISwitch PowerS[]          = {{"Power On" , ISS_OFF},{"Power Off", ISS_ON}};

static ISwitch SlewModeS[]       = {{"Slew", ISS_ON}, {"Find", ISS_OFF}, {"Center", ISS_OFF}, {"Guide", ISS_OFF}};

static ISwitch OnCoordSetS[]     = {{ "Slew", ISS_ON }, { "Sync", ISS_OFF }};
static ISwitch abortSlewS[]      = {{"Abort Slew/Track", ISS_OFF }};
static ISwitch MovementS[]       = {{"North", ISS_OFF}, {"West", ISS_OFF}, {"East", ISS_OFF}, {"South", ISS_OFF}};
static ISwitch haltMoveS[]       = {{"Northward", ISS_OFF}, {"Westward", ISS_OFF}, {"Eastward", ISS_OFF}, {"Southward", ISS_OFF}};


/* Fundamental group */
static ISwitches PowerSw	 = { mydev, "POWER" , PowerS, NARRAY(PowerS), ILS_IDLE, 0, COMM_GROUP };
static IText Port		 = { mydev, "Ports", NULL, ILS_IDLE, 0, COMM_GROUP};

/* Basic data group */
static INumber RA          = { mydev, "RA", NULL, ILS_IDLE, 0 , BASIC_GROUP};
static INumber DEC         = { mydev, "DEC", NULL, ILS_IDLE, 0 , BASIC_GROUP};

/* Movement group */
static ISwitches OnCoordSetSw    = { mydev, "ONCOORDSET", OnCoordSetS, NARRAY(OnCoordSetS), ILS_IDLE, 0, BASIC_GROUP};
static ISwitches SlewModeSw      = { mydev, "Slew rate", SlewModeS, NARRAY(SlewModeS), ILS_IDLE, 0, MOVE_GROUP};
static ISwitches abortSlewSw     = { mydev, "ABORTSLEW", abortSlewS, NARRAY(abortSlewS), ILS_IDLE, 0, MOVE_GROUP};
static ISwitches MovementSw      = { mydev, "Move toward", MovementS, NARRAY(MovementS), ILS_IDLE, 0, MOVE_GROUP};
static ISwitches haltMoveSw      = { mydev, "Halt movement", haltMoveS, NARRAY(haltMoveS), ILS_IDLE, 0, MOVE_GROUP};



/* send client definitions of all properties */
void ISGetProperties (const char *dev) {telescope->ISGetProperties(dev);}
void ISNewText (IText *t) {telescope->ISNewText(t);}
void ISNewNumber (INumber *n) {telescope->ISNewNumber(n);}
void ISNewSwitch (ISwitches *s) {telescope->ISNewSwitch(s);}
void ISPoll () {telescope->ISPoll();}

/**************************************************
*** LX200 Generic Implementation
***************************************************/

CelestronGPS::CelestronGPS()
{
   ICPollMe (POLLMS);
   RA.nstr     = strcpy (new char[12] , " 00:00:00");
   DEC.nstr    = strcpy (new char[12] , " 00:00:00");
   Port.text         = strcpy (new char[32] , "/dev/ttyS0");

   targetRA  = 0;
   targetDEC = 0;

   // Children call parent routines, this is the default
   fprintf(stderr , "initilizaing from Celestron GPS device...\n");

}

void CelestronGPS::ISGetProperties(const char *dev)
{

 if (dev && strcmp (mydev, dev))
    return;

  ICDefSwitches (&PowerSw, "Power", ISP_W, IR_1OFMANY);
  ICDefText     (&Port , "Port", IP_RW);

  ICDefNumber (&RA, "RA H:M:S", IP_RW, NULL);
  ICDefNumber (&DEC, "DEC D:M:S", IP_RW, NULL);
  ICDefSwitches (&OnCoordSetSw, "On Set", ISP_W, IR_1OFMANY);

  ICDefSwitches (&abortSlewSw, "Abort", ISP_W, IR_1OFMANY);
  ICDefSwitches (&SlewModeSw, "Slew rate", ISP_W, IR_1OFMANY);
  ICDefSwitches (&MovementSw, "Move toward", ISP_W, IR_1OFMANY);
  ICDefSwitches (&haltMoveSw, "Halt movement", ISP_W, IR_1OFMANY);

}

void CelestronGPS::ISNewText(IText *t)
{
	// ignore if not ours //
	if (strcmp (t->dev, mydev))
	    return;

	if (!strcmp(t->name, Port.name) )
	{
	  Port.s = ILS_OK;
	  strcpy(Port.text, t->text);
	  ICSetText (&Port, NULL);
	  return;
	}
}

int CelestronGPS::handleCoordSet()
{

  int i=0;
  char RAbuffer[64];
  char Decbuffer[64];

  switch (lastSet)
  {

    // Slew
    case 0:

	  if (OnCoordSetSw.s == ILS_BUSY)
	  {
	     StopNSEW();

	     // sleep for 100 mseconds
	     usleep(100000);
	  }

	  if ((i = SlewToCoords(targetRA, targetDEC)))
	  {
	    slewError(i);
	    return (-1);
	  }

	  formatSex(targetRA, RAbuffer, XXYYZZ);
	  formatSex(targetDEC, Decbuffer, SXXYYZZ);
	  OnCoordSetSw.s = ILS_BUSY;
	  ICSetSwitch(&OnCoordSetSw, "Slewing to RA %s - DEC %s", RAbuffer, Decbuffer);
	  break;


  // Sync
  case 1:

	  OnCoordSetSw.s = ILS_OK;
	  SyncToCoords(targetRA, targetDEC);

          formatSex(targetRA, RAbuffer, XXYYZZ);
	  formatSex(targetDEC, Decbuffer, SXXYYZZ);
	  strcpy(RA.nstr, RAbuffer);
	  strcpy(DEC.nstr, Decbuffer);
          RA.s = ILS_OK;
	  DEC.s = ILS_OK;
	  ICSetNumber(&RA, NULL);
	  ICSetNumber(&DEC, NULL);
	  ICSetSwitch(&OnCoordSetSw, "Synchronization successful.");
	  break;
   }

   return (0);

}

void CelestronGPS::ISNewNumber (INumber *n)
{

        float d, h, m , s;

	// ignore if not ours //
	if (strcmp (n->dev, mydev))
	    return;

	if ( !strcmp (n->name, RA.name) && !validateSex(n->nstr, &h, &m, &s ))
	{
	  if (h < 0 || h > 24)
	  {
	   RA.s = ILS_IDLE;
	   ICSetNumber (&RA, "RA coordinate out of range (0 to 24 hours)");
	   return;
	  }

	  if (checkPower())
	  {
	    RA.s = ILS_IDLE;
	    ICSetNumber(&RA, NULL);
	    return;
	  }

	   RA.s = ILS_BUSY;
	   getSex(n->nstr, &targetRA);

	   if (handleCoordSet())
	   {
	        strcpy(RA.nstr, n->nstr);
	        RA.s = ILS_IDLE;
	    	ICSetNumber(&RA, NULL);
	   }

         } // end RA set


	if ( !strcmp (n->name, DEC.name) && !validateSex(n->nstr, &d, &m, &s))
	{
	  if (d < -90.0 || d > 90.0)
	  {
	   DEC.s = ILS_IDLE;
	   ICSetNumber (&DEC, "DEC coordinate out of range (-90 to +90 degrees)");
	   return;
	  }

	  if (checkPower())
	  {
	    DEC.s = ILS_IDLE;
	    ICSetNumber(&DEC, NULL);
	    return;
	  }

	  DEC.s = ILS_BUSY;
	  getSex(n->nstr, &targetDEC);

	  if (handleCoordSet())
	  {
	        strcpy(DEC.nstr, n->nstr);
	        DEC.s = ILS_IDLE;
	        ICSetNumber(&DEC, NULL);
	  }


	} // end DEC set


}

void CelestronGPS::ISNewSwitch(ISwitches *s)
{

	int index[16];

	// ignore if not ours //
	if (strcmp (s->dev, mydev))
	    return;

	// FIRST Switch ALWAYS for power
	if (!strcmp (s->name, PowerSw.name))
	{
   	 powerTelescope(s);
	 return;
	}

	if (!validateSwitch(s, &OnCoordSetSw, NARRAY(OnCoordSetS), index, 0))
	{
	  lastSet = index[0];
	  handleCoordSet();
	  return;
	}

	// Abort Slew
	if (!strcmp (s->name, abortSlewSw.name))
	{
	  if (checkPower())
	  {
	    abortSlewSw.s = ILS_IDLE;
	    ICSetSwitch(&abortSlewSw, NULL);
	    return;
	  }

	    if (OnCoordSetSw.s == ILS_BUSY || OnCoordSetSw.s == ILS_OK)
	    {
	    	StopNSEW();
		abortSlewSw.s = ILS_OK;
		OnCoordSetSw.s = ILS_IDLE;
		ICSetSwitch(&abortSlewSw, "Slew aborted");
		ICSetSwitch(&OnCoordSetSw, NULL);
            }
	    else if (MovementSw.s == ILS_BUSY)
	    {
	        StopSlew(NORTH);
		StopSlew(WEST);
		StopSlew(EAST);
		StopSlew(SOUTH);
		abortSlewSw.s = ILS_OK;
		MovementSw.s = ILS_IDLE;
		ICSetSwitch(&abortSlewSw, "Slew aborted");
		ICSetSwitch(&MovementSw, NULL);
	    }
	    else
	    {
	        abortSlewSw.s = ILS_IDLE;
	        ICSetSwitch(&abortSlewSw, NULL);
	    }

	    return;
	}

	if (!validateSwitch(s, &SlewModeSw, NARRAY(SlewModeS), index, 1))
	{
	  SetRate(index[0]);
	  SlewModeSw.s = ILS_OK;
	  ICSetSwitch(&SlewModeSw, NULL);
	  return;
	}

	if (!validateSwitch(s, &MovementSw, NARRAY(MovementS), index,1))
	{
          if (lastMove[index[0]])
	    return;

	  lastMove[index[0]] = 1;

	  StartSlew(index[0]);

	  for (uint i=0; i < 4; i++)
	    MovementSw.sw[i].s = lastMove[i] == 0 ? ISS_OFF : ISS_ON;

	  MovementSw.s = ILS_BUSY;
	  ICSetSwitch(&MovementSw, "Moving %s...", Direction[index[0]]);
	  return;
	}

	if (!validateSwitch(s, &haltMoveSw, NARRAY(haltMoveS), index, 1))
	{
	  if (MovementSw.s == ILS_BUSY)
	  {
		StopSlew(index[0]);
		lastMove[index[0]] = 0;

		if (!lastMove[0] && !lastMove[1] && !lastMove[2] && !lastMove[3])
		  MovementSw.s = ILS_IDLE;

		for (uint i=0; i < 4; i++)
		{
	    	   haltMoveSw.sw[i].s = ISS_OFF;
		   MovementSw.sw[i].s = lastMove[i] == 0 ? ISS_OFF : ISS_ON;
		}

		RA.s = ILS_IDLE;
		DEC.s = ILS_IDLE;
                haltMoveSw.s = ILS_IDLE;

		ICSetSwitch(&haltMoveSw, "Moving toward %s aborted", Direction[index[0]]);
	  	ICSetSwitch(&MovementSw, NULL);
	  }
	  else
	  {
	        haltMoveSw.sw[index[0]].s = ISS_OFF;
	     	haltMoveSw.s = ILS_IDLE;
	        ICSetSwitch(&haltMoveSw, NULL);
	  }
	  return;
	 }

}

void CelestronGPS::ISPoll()
{

       double currentRA, currentDEC;

	switch (OnCoordSetSw.s)
	{
	case ILS_IDLE:
	     break;

	case ILS_BUSY:

	    fprintf(stderr , "Getting LX200 RA, DEC...\n");
	    currentRA = GetRA();
	    currentDEC = GetDec();

	    formatSex (currentRA, RA.nstr, XXYYZZ);
	    formatSex (currentDEC, DEC.nstr, SXXYYZZ);

	    fprintf(stderr, "targetRA is %f, currentRA is %f\n", (float) targetRA, (float) currentRA);
	    fprintf(stderr, "targetDEC is %f, currentDEC is %f\n****************************\n", (float) targetDEC, (float) currentDEC);


	    if (!CheckCoords( targetRA, targetDEC))
	    {

		OnCoordSetSw.s = ILS_OK;
		currentRA = targetRA;
		currentDEC = targetDEC;

		formatSex (targetRA, RA.nstr, XXYYZZ);
		formatSex (targetDEC, DEC.nstr, SXXYYZZ);

		RA.s = ILS_OK;
		DEC.s = ILS_OK;
		ICSetNumber (&RA, NULL);
		ICSetNumber (&DEC, NULL);
		ICSetSwitch (&OnCoordSetSw, "Slew is complete");


	    } else
	    {
		ICSetNumber (&RA, NULL);
		ICSetNumber (&DEC, NULL);
	    }
	    break;

	case ILS_OK:
	    break;

	case ILS_ALERT:
	    break;
	}

	switch (MovementSw.s)
	{
	  case ILS_IDLE:
	   break;
	 case ILS_BUSY:
	     currentRA = GetRA();
	     currentDEC = GetDec();
	     formatSex(currentRA, RA.nstr, XXYYZZ);
	     formatSex(currentDEC, DEC.nstr, SXXYYZZ);
	     ICSetNumber (&RA, NULL);
	     ICSetNumber (&DEC, NULL);
	     break;
	 case ILS_OK:
	   break;
	 case ILS_ALERT:
	   break;
	 }

}

int CelestronGPS::checkPower()
{

  if (PowerSw.s != ILS_OK)
  {
    ICMessage (mydev, "Cannot change a property while the telescope is offline");
    return -1;
  }

  return 0;

}

int CelestronGPS::validateSwitch(ISwitches *clientSw, ISwitches *driverSw, int driverArraySize, int index[], int validatePower)
{
//fprintf(stderr , "IN validate switch\n");
  int i, j;

  if (!strcmp (clientSw->name, driverSw->name))
  {
	  // check if the telescope is connected //
	 if (validatePower && (checkPower()))
	 {
	     driverSw->s = ILS_IDLE;
	     ICSetSwitch (driverSw, NULL);
	     return -1;
	 }

	 for (i = 0, j =0 ; i < driverArraySize; i++)
	 {
		if (!strcmp (clientSw->sw[0].name, driverSw->sw[i].name))
		{
		    index[j++] = i;
		    driverSw->sw[i].s = ISS_ON;
		} else
		    driverSw->sw[i].s = ISS_OFF;
	  }
	   return 0;
  }

    return -1;
 }



void CelestronGPS::getBasicData()
{

  formatSex ( (targetRA = GetRA()), RA.nstr, XXYYZZ);
  formatSex ( (targetDEC = GetDec()), DEC.nstr, SXXYYZZ);

  ICSetNumber(&RA, NULL);
  ICSetNumber(&DEC, NULL);

}

void CelestronGPS::powerTelescope(ISwitches* s)
{
   fprintf(stderr , "In POWER\n");
    for (uint i= 0; i < NARRAY(PowerS); i++)
    {
        if (!strcmp (s->sw[0].name, PowerS[i].name))
	   PowerS[i].s = ISS_ON;
	else
	   PowerS[i].s = ISS_OFF;
    }

     if ((PowerSw.sw[0].s == ISS_ON) && ConnectTel(Port.text) < 0)
     {
        PowerSw.s = ILS_IDLE;
	PowerS[0].s = ISS_OFF;
	PowerS[1].s = ISS_ON;

	ICSetSwitch (&PowerSw, "Telescope is not connected to the serial/usb port");
	DisconnectTel();
     }
     else if (PowerSw.sw[0].s == ISS_ON)
     {
        PowerSw.s = ILS_OK;
	ICSetSwitch (&PowerSw, "Telescope is online. Retrieving basic data...");
	getBasicData();
     }
     else
     {
         PowerSw.s = ILS_IDLE;
	 ICSetSwitch (&PowerSw, "Telescope is offline.");
	 DisconnectTel();

     }

}

void CelestronGPS::slewError(int slewCode)
{
    OnCoordSetSw.s = ILS_IDLE;

    switch (slewCode)
    {
      case 1:
       ICSetSwitch (&OnCoordSetSw, "Invalid newDec in SlewToCoords");
       break;
      case 2:
       ICSetSwitch (&OnCoordSetSw, "RA count overflow in SlewToCoords");
       break;
      case 3:
       ICSetSwitch (&OnCoordSetSw, "Dec count overflow in SlewToCoords");
       break;
      case 4:
       ICSetSwitch (&OnCoordSetSw, "No acknowledgement from telescope after SlewToCoords");
       break;
      default:
       ICSetSwitch (&OnCoordSetSw, "Unknown error");
       break;
    }

}
