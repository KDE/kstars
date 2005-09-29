#if 0
    LX200 Basic Driver
    Copyright (C) 2005 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <unistd.h>
#include <time.h>

#include "indicom.h"
#include "lx200driver.h"
#include "lx200basic.h"

/*
** Return the timezone offset in hours (as a double, so fractional
** hours are possible, for instance in Newfoundland). Also sets
** daylight on non-Linux systems to record whether DST is in effect.
*/


LX200Basic *telescope = NULL;
extern char* me;

#define BASIC_GROUP	"Main Control"

#define currentRA	EqN[0].value
#define currentDEC	EqN[1].value

#define RA_THRESHOLD	0.01
#define DEC_THRESHOLD	0.05
#define LX200_SLEW	0
#define LX200_TRACK	1
#define LX200_SYNC	2
#define LX200_PARK	3

static void ISPoll(void *);
static void retryConnection(void *);

/*INDI controls */


/* send client definitions of all properties */
void ISInit()
{
  static int isInit=0;

 if (isInit)
  return;

 isInit = 1;
  
  telescope = new LX200Basic();
  IEAddTimer (POLLMS, ISPoll, NULL);
}

void ISGetProperties (const char *dev)
{
 ISInit(); 
 telescope->ISGetProperties(dev);
}

void ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
 ISInit();
 telescope->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
 ISInit();
 telescope->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
 ISInit();
 telescope->ISNewNumber(dev, name, values, names, n);
}

void ISPoll (void */*p*/)
{
 telescope->ISPoll(); 
 IEAddTimer (POLLMS, ISPoll, NULL);
}

void ISNewBLOB (const char */*dev*/, const char */*name*/, int */*sizes[]*/, char **/*blobs[]*/, char **/*formats[]*/, char **/*names[]*/, int /*n*/)
{}

/**************************************************
*** AP Mount
***************************************************/

LX200Basic::LX200Basic()
{
   struct tm *utp;
   time_t t;
   time (&t);
   utp = gmtime (&t);

   initProperties();

   lastSet        = -1;
   simulation     = false;
   targetRA       = 0;
   targetDEC      = 0;
   lastRA 	  = 0;
   lastDEC	  = 0;
   currentSet     = 0;
   UTCOffset      = 0;

   localTM = new tm;
   
   utp->tm_mon  += 1;
   utp->tm_year += 1900;
   JD = UTtoJD(utp);
   
   IDLog("Julian Day is %g\n", JD);
   IDLog("Initilizing from LX200 Basic device...\n");
   IDLog("Driver Version: 2005-07-20\n");
 
   //enableSimulation(true);  
}

void LX200Basic::initProperties()
{

  fillSwitch(&PowerS[0], "CONNECT", "Connect", ISS_OFF);
  fillSwitch(&PowerS[1], "DISCONNECT", "Disconnect", ISS_ON);
  fillSwitchVector(&PowerSP, PowerS, NARRAY(PowerS), mydev, "CONNECTION", "Connection", BASIC_GROUP, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

  fillSwitch(&OnCoordSetS[0], "SLEW", "Slew", ISS_ON);
  fillSwitch(&OnCoordSetS[1], "TRACK", "Track", ISS_OFF);
  fillSwitch(&OnCoordSetS[2], "SYNC", "Sync", ISS_OFF);
  fillSwitchVector(&OnCoordSetSP, OnCoordSetS, NARRAY(OnCoordSetS), mydev, "ON_COORD_SET", "On Set", BASIC_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

   fillSwitch(&AbortSlewS[0], "ABORT", "Abort", ISS_OFF);
   fillSwitchVector(&AbortSlewSP, AbortSlewS, NARRAY(AbortSlewS), mydev, "ABORT_MOTION", "Abort Slew/Track", BASIC_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

  fillText(&PortT[0], "PORT", "Port", "/dev/ttyS0");
  fillTextVector(&PortTP, PortT, NARRAY(PortT), mydev, "DEVICE_PORT", "Ports", BASIC_GROUP, IP_RW, 0, IPS_IDLE);

  fillText(&ObjectT[0], "OBJECT_NAME", "Name", "--");
  fillTextVector(&ObjectTP, ObjectT, NARRAY(ObjectT), mydev, "OBJECT_INFO", "Object", BASIC_GROUP, IP_RW, 0, IPS_IDLE);

   fillNumber(&EqN[0], "RA", "RA  H:M:S", "%10.6m",  0., 24., 0., 0.);
   fillNumber(&EqN[1], "DEC", "Dec D:M:S", "%10.6m", -90., 90., 0., 0.);
   fillNumberVector(&EqNP, EqN, NARRAY(EqN), mydev, "EQUATORIAL_EOD_COORD" , "Equatorial JNow", BASIC_GROUP, IP_RW, 0, IPS_IDLE);

   
}

void LX200Basic::ISGetProperties(const char *dev)
{

 if (dev && strcmp (mydev, dev))
    return;

  // Main Control
  IDDefSwitch(&PowerSP, NULL);
  IDDefText(&PortTP, NULL);
  IDDefText(&ObjectTP, NULL);
  IDDefNumber(&EqNP, NULL);
  IDDefSwitch(&OnCoordSetSP, NULL);
  IDDefSwitch(&AbortSlewSP, NULL);
  
}

void LX200Basic::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int /*n*/)
{
	IText *tp;

	// ignore if not ours 
	if (strcmp (dev, mydev))
	    return;

	// Port name
	if (!strcmp(name, PortTP.name) )
	{
	  PortTP.s = IPS_OK;
	  tp = IUFindText( &PortTP, names[0] );
	  if (!tp)
	   return;

	   IUSaveText(tp, texts[0]);
 	   IDSetText (&PortTP, NULL);
	  return;
	}

       if (!strcmp (name, ObjectTP.name))
       {
	  if (checkPower(&ObjectTP))
	   return;

          IUSaveText(&ObjectT[0], texts[0]);
          ObjectTP.s = IPS_OK;
          IDSetText(&ObjectTP, NULL);
          return;
       }
          
}


void LX200Basic::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
	int err;
	double newRA =0, newDEC =0;
	
	// ignore if not ours //
	if (strcmp (dev, mydev))
	    return;

	if (!strcmp (name, EqNP.name))
	{
	  int i=0, nset=0;

	  if (checkPower(&EqNP))
	   return;

	    for (nset = i = 0; i < n; i++)
	    {
		INumber *eqp = IUFindNumber (&EqNP, names[i]);
		if (eqp == &EqN[0])
		{
                    newRA = values[i];
		    nset += newRA >= 0 && newRA <= 24.0;
		} else if (eqp == &EqN[1])
		{
		    newDEC = values[i];
		    nset += newDEC >= -90.0 && newDEC <= 90.0;
		}
	    }

	  if (nset == 2)
	  {
	   char RAStr[32], DecStr[32];

	   fs_sexa(RAStr, newRA, 2, 3600);
	   fs_sexa(DecStr, newDEC, 2, 3600);
	  
	   IDLog("We received JNow RA %g - DEC %g\n", newRA, newDEC);
	   IDLog("We received JNow RA %s - DEC %s\n", RAStr, DecStr);
	   
	   if ( (err = setObjectRA(newRA)) < 0 || ( err = setObjectDEC(newDEC)) < 0)
	   {
	     handleError(&EqNP, err, "Setting RA/DEC");
	     return;
	   } 
	   
	   targetRA  = newRA;
	   targetDEC = newDEC;
	   
	   if (handleCoordSet())
	   {
	     EqNP.s = IPS_IDLE;
	     IDSetNumber(&EqNP, NULL);
	     
	   }
	} // end nset
	else
	{
		EqNP.s = IPS_IDLE;
		IDSetNumber(&EqNP, "RA or Dec missing or invalid");
	}

	    return;
     } /* end EqNP */


}

void LX200Basic::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
	// ignore if not ours //
	if (strcmp (mydev, dev))
	    return;

	// Connection
	if (!strcmp (name, PowerSP.name))
	{
	 IUResetSwitches(&PowerSP);
	 IUUpdateSwitches(&PowerSP, states, names, n);
   	 powerTelescope();
	 return;
	}

	// Coord set
	if (!strcmp(name, OnCoordSetSP.name))
	{
  	  if (checkPower(&OnCoordSetSP))
	   return;

	  IUResetSwitches(&OnCoordSetSP);
	  IUUpdateSwitches(&OnCoordSetSP, states, names, n);
	  currentSet = getOnSwitch(&OnCoordSetSP);
	  OnCoordSetSP.s = IPS_OK;
	  IDSetSwitch(&OnCoordSetSP, NULL);
	}
	  
	// Abort Slew
	if (!strcmp (name, AbortSlewSP.name))
	{
	  if (checkPower(&AbortSlewSP))
	  {
	    AbortSlewSP.s = IPS_IDLE;
	    IDSetSwitch(&AbortSlewSP, NULL);
	    return;
	  }
	  
	  IUResetSwitches(&AbortSlewSP);
	  abortSlew();

	    if (EqNP.s == IPS_BUSY)
	    {
		AbortSlewSP.s = IPS_OK;
		EqNP.s       = IPS_IDLE;
		IDSetSwitch(&AbortSlewSP, "Slew aborted.");
		IDSetNumber(&EqNP, NULL);
            }

	    return;
	}

}

void LX200Basic::handleError(ISwitchVectorProperty *svp, int err, const char *msg)
{
  
  svp->s = IPS_ALERT;
  
  /* First check to see if the telescope is connected */
    if (testTelescope())
    {
      /* The telescope is off locally */
      PowerS[0].s = ISS_OFF;
      PowerS[1].s = ISS_ON;
      PowerSP.s = IPS_BUSY;
      IDSetSwitch(&PowerSP, "Telescope is not responding to commands, will retry in 10 seconds.");
      
      IDSetSwitch(svp, NULL);
      IEAddTimer(10000, retryConnection, NULL);
      return;
    }
    
   /* If the error is a time out, then the device doesn't support this property or busy*/
      if (err == -2)
      {
       svp->s = IPS_ALERT;
       IDSetSwitch(svp, "Device timed out. Current device may be busy or does not support %s. Will retry again.", msg);
      }
      else
    /* Changing property failed, user should retry. */
       IDSetSwitch( svp , "%s failed.", msg);
       
       fault = true;
}

void LX200Basic::handleError(INumberVectorProperty *nvp, int err, const char *msg)
{
  
  nvp->s = IPS_ALERT;
  
  /* First check to see if the telescope is connected */
    if (testTelescope())
    {
      /* The telescope is off locally */
      PowerS[0].s = ISS_OFF;
      PowerS[1].s = ISS_ON;
      PowerSP.s = IPS_BUSY;
      IDSetSwitch(&PowerSP, "Telescope is not responding to commands, will retry in 10 seconds.");
      
      IDSetNumber(nvp, NULL);
      IEAddTimer(10000, retryConnection, NULL);
      return;
    }
    
   /* If the error is a time out, then the device doesn't support this property */
      if (err == -2)
      {
       nvp->s = IPS_ALERT;
       IDSetNumber(nvp, "Device timed out. Current device may be busy or does not support %s. Will retry again.", msg);
      }
      else
    /* Changing property failed, user should retry. */
       IDSetNumber( nvp , "%s failed.", msg);
       
       fault = true;
}

void LX200Basic::handleError(ITextVectorProperty *tvp, int err, const char *msg)
{
  
  tvp->s = IPS_ALERT;
  
  /* First check to see if the telescope is connected */
    if (testTelescope())
    {
      /* The telescope is off locally */
      PowerS[0].s = ISS_OFF;
      PowerS[1].s = ISS_ON;
      PowerSP.s = IPS_BUSY;
      IDSetSwitch(&PowerSP, "Telescope is not responding to commands, will retry in 10 seconds.");
      
      IDSetText(tvp, NULL);
      IEAddTimer(10000, retryConnection, NULL);
      return;
    }
    
   /* If the error is a time out, then the device doesn't support this property */
      if (err == -2)
      {
       tvp->s = IPS_ALERT;
       IDSetText(tvp, "Device timed out. Current device may be busy or does not support %s. Will retry again.", msg);
      }
       
      else
    /* Changing property failed, user should retry. */
       IDSetText( tvp , "%s failed.", msg);
       
       fault = true;
}

 void LX200Basic::correctFault()
 {
 
   fault = false;
   IDMessage(mydev, "Telescope is online.");
   
 }

bool LX200Basic::isTelescopeOn(void)
{
  if (simulation) return true;
  
  return (PowerSP.sp[0].s == ISS_ON);
}

static void retryConnection(void * p)
{
  p=p;
  
  if (testTelescope())
	telescope->connectionLost();
  else
	telescope->connectionResumed();
}

void LX200Basic::ISPoll()
{
        double dx, dy;
	int err=0;
	
	if (!isTelescopeOn())
	 return;

	switch (EqNP.s)
	{
	case IPS_IDLE:
	getLX200RA(&currentRA);
	getLX200DEC(&currentDEC);
	
        if ( fabs (currentRA - lastRA) > 0.01 || fabs (currentDEC - lastDEC) > 0.01)
	{
	        lastRA = currentRA;
		lastDEC = currentDEC;
		IDSetNumber (&EqNP, NULL);
	}
        break;

        case IPS_BUSY:
	    getLX200RA(&currentRA);
	    getLX200DEC(&currentDEC);
	    dx = targetRA - currentRA;
	    dy = targetDEC - currentDEC;

	    IDLog("targetRA is %g, currentRA is %g\n", targetRA, currentRA);
	    IDLog("targetDEC is %g, currentDEC is %g\n*************************\n", targetDEC, currentDEC);

	    // Wait until acknowledged or within threshold
	    if (fabs(dx) <= RA_THRESHOLD && fabs(dy) <= DEC_THRESHOLD)
	    {
		
	       lastRA  = currentRA;
	       lastDEC = currentDEC;
	       IUResetSwitches(&OnCoordSetSP);
	       OnCoordSetSP.s = IPS_OK;
	       EqNP.s = IPS_OK;
	       IDSetNumber (&EqNP, NULL);

		switch (currentSet)
		{
		  case LX200_SLEW:
		  	OnCoordSetSP.sp[0].s = ISS_ON;
		  	IDSetSwitch (&OnCoordSetSP, "Slew is complete.");
		  	break;
		  
		  case LX200_TRACK:
		  	OnCoordSetSP.sp[1].s = ISS_ON;
		  	IDSetSwitch (&OnCoordSetSP, "Slew is complete. Tracking...");
			break;
		  
		  case LX200_SYNC:
		  	break;
		}
		  
	    } else
		IDSetNumber (&EqNP, NULL);
	    break;

	case IPS_OK:
	  
	if ( (err = getLX200RA(&currentRA)) < 0 || (err = getLX200DEC(&currentDEC)) < 0)
	{
	  handleError(&EqNP, err, "Getting RA/DEC");
	  return;
	}
	
	if (fault)
	  correctFault();
	
	if ( (currentRA != lastRA) || (currentDEC != lastDEC))
	{
	  	lastRA  = currentRA;
		lastDEC = currentDEC;
		IDSetNumber (&EqNP, NULL);
	}
        break;


	case IPS_ALERT:
	    break;
	}

}

void LX200Basic::getBasicData()
{
  // Get current RA/DEC
  getLX200RA(&currentRA);
  getLX200DEC(&currentDEC);
  targetRA = currentRA;
  targetDEC = currentDEC;

  IDSetNumber (&EqNP, NULL);  
}

int LX200Basic::handleCoordSet()
{

  int  err;
  char syncString[256];
  char RAStr[32], DecStr[32];
  double dx, dy;
  
  switch (currentSet)
  {

    // Slew
    case LX200_SLEW:
          lastSet = LX200_SLEW;
	  if (EqNP.s == IPS_BUSY)
	  {
	     IDLog("Aboring Slew\n");
	     abortSlew();

	     // sleep for 100 mseconds
	     usleep(100000);
	  }

	  if ((err = Slew()))
	  {
	    slewError(err);
	    return (-1);
	  }

	  EqNP.s = IPS_BUSY;
	  fs_sexa(RAStr, targetRA, 2, 3600);
	  fs_sexa(DecStr, targetDEC, 2, 3600);
	  IDSetNumber(&EqNP, "Slewing to JNow RA %s - DEC %s", RAStr, DecStr);
	  IDLog("Slewing to JNow RA %s - DEC %s\n", RAStr, DecStr);
	  break;

     // Track
     case LX200_TRACK:
          IDLog("We're in LX200_TRACK\n");
          if (EqNP.s == IPS_BUSY)
	  {
	     IDLog("Aboring Slew\n");
	     abortSlew();

	     // sleep for 200 mseconds
	     usleep(200000);
	  }

	  dx = fabs ( targetRA - currentRA );
	  dy = fabs (targetDEC - currentDEC);

	  
	  if (dx >= TRACKING_THRESHOLD || dy >= TRACKING_THRESHOLD) 
	  {
	        IDLog("Exceeded Tracking threshold, will attempt to slew to the new target.\n");
		IDLog("targetRA is %g, currentRA is %g\n", targetRA, currentRA);
	        IDLog("targetDEC is %g, currentDEC is %g\n*************************\n", targetDEC, currentDEC);

          	if ((err = Slew()))
	  	{
	    		slewError(err);
	    		return (-1);
	  	}

		fs_sexa(RAStr, targetRA, 2, 3600);
	        fs_sexa(DecStr, targetDEC, 2, 3600);
		EqNP.s = IPS_BUSY;
		IDSetNumber(&EqNP, "Slewing to JNow RA %s - DEC %s", RAStr, DecStr);
		IDLog("Slewing to JNow RA %s - DEC %s\n", RAStr, DecStr);
	  }
	  else
	  {
	    IDLog("Tracking called, but tracking threshold not reached yet.\n");
	    EqNP.s = IPS_OK;
	    EqNP.np[0].value = currentRA;
	    EqNP.np[1].value = currentDEC;

	    if (lastSet != LX200_TRACK)
	      IDSetNumber(&EqNP, "Tracking...");
	    else
	      IDSetNumber(&EqNP, NULL);
	  }
	  lastSet = LX200_TRACK;
      break;

    // Sync
    case LX200_SYNC:
          lastSet = LX200_SYNC;
	  EqNP.s = IPS_IDLE;
	   
	  if ( ( err = Sync(syncString) < 0) )
	  {
	        IDSetNumber( &EqNP , "Synchronization failed.");
		return (-1);
	  }

	  EqNP.s = IPS_OK;
	  IDLog("Synchronization successful %s\n", syncString);
	  IDSetNumber(&EqNP, "Synchronization successful.");
	  break;
    }

   return (0);

}

int LX200Basic::getOnSwitch(ISwitchVectorProperty *sp)
{
 for (int i=0; i < sp->nsp ; i++)
     if (sp->sp[i].s == ISS_ON)
      return i;

 return -1;
}


int LX200Basic::checkPower(ISwitchVectorProperty *sp)
{
  if (simulation) return 0;
  
  if (PowerSP.s != IPS_OK)
  {
    if (!strcmp(sp->label, ""))
    	IDMessage (mydev, "Cannot change property %s while the telescope is offline.", sp->name);
    else
        IDMessage (mydev, "Cannot change property %s while the telescope is offline.", sp->label);
	
    sp->s = IPS_IDLE;
    IDSetSwitch(sp, NULL);
    return -1;
  }

  return 0;
}

int LX200Basic::checkPower(INumberVectorProperty *np)
{
  if (simulation) return 0;
  
  if (PowerSP.s != IPS_OK)
  {
    
    if (!strcmp(np->label, ""))
    	IDMessage (mydev, "Cannot change property %s while the telescope is offline.", np->name);
    else
        IDMessage (mydev, "Cannot change property %s while the telescope is offline.", np->label);
	
    np->s = IPS_IDLE;
    IDSetNumber(np, NULL);
    return -1;
  }

  return 0;

}

int LX200Basic::checkPower(ITextVectorProperty *tp)
{

  if (simulation) return 0;
  
  if (PowerSP.s != IPS_OK)
  {
    if (!strcmp(tp->label, ""))
    	IDMessage (mydev, "Cannot change property %s while the telescope is offline.", tp->name);
    else
        IDMessage (mydev, "Cannot change property %s while the telescope is offline.", tp->label);
	
    tp->s = IPS_IDLE;
    IDSetText(tp, NULL);
    return -1;
  }

  return 0;

}

void LX200Basic::powerTelescope()
{
     switch (PowerSP.sp[0].s)
     {
      case ISS_ON:  
	
        if (simulation)
	{
	  PowerSP.s = IPS_OK;
	  IDSetSwitch (&PowerSP, "Simulated telescope is online.");
	  return;
	}
	
         if (Connect(PortT[0].text))
	 {
	   PowerS[0].s = ISS_OFF;
	   PowerS[1].s = ISS_ON;
	   IDSetSwitch (&PowerSP, "Error connecting to port %s\n", PortT[0].text);
	   return;
	 }

	 if (testTelescope())
	 {   
	   PowerS[0].s = ISS_OFF;
	   PowerS[1].s = ISS_ON;
	   IDSetSwitch (&PowerSP, "Error connecting to Telescope. Telescope is offline.");
	   return;
	 }

        IDLog("telescope test successfful\n");
	PowerSP.s = IPS_OK;
	IDSetSwitch (&PowerSP, "Telescope is online. Retrieving basic data...");
	getBasicData();
	break;

     case ISS_OFF:
         PowerS[0].s = ISS_OFF;
	 PowerS[1].s = ISS_ON;
         PowerSP.s = IPS_IDLE;
	 if (simulation)
         {
	    IDSetSwitch (&PowerSP, "Simulated Telescope is offline.");
	    return;
         }
         IDSetSwitch (&PowerSP, "Telescope is offline.");
	 IDLog("Telescope is offline.");
         
	 Disconnect();
	 break;

    }

}

void LX200Basic::slewError(int slewCode)
{
    OnCoordSetSP.s = IPS_IDLE;

    if (slewCode == 1)
	IDSetSwitch (&OnCoordSetSP, "Object below horizon.");
    else if (slewCode == 2)
	IDSetSwitch (&OnCoordSetSP, "Object below the minimum elevation limit.");
    else
        IDSetSwitch (&OnCoordSetSP, "Slew failed.");
    

}

void LX200Basic::enableSimulation(bool enable)
{
   simulation = enable;
   
   if (simulation)
     IDLog("Warning: Simulation is activated.\n");
   else
     IDLog("Simulation is disabled.\n");
}

void LX200Basic::connectionLost()
{
    PowerSP.s = IPS_IDLE;
    IDSetSwitch(&PowerSP, "The connection to the telescope is lost.");
    return;
 
}

void LX200Basic::connectionResumed()
{
  PowerS[0].s = ISS_ON;
  PowerS[1].s = ISS_OFF;
  PowerSP.s = IPS_OK;
   
  IDSetSwitch(&PowerSP, "The connection to the telescope has been resumed.");
}


