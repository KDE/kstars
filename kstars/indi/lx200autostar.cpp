/*
    LX200 Autostar
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
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "lx200autostar.h"
#include "lx200driver.h"

#define FirmwareGroup "Firmware data"
#define FOCUS_GROUP   "Focus Control"


extern LX200Generic *telescope;
extern int MaxReticleFlashRate;
extern ITextVectorProperty Time;
extern INumberVectorProperty SDTime;
extern INumberVectorProperty eqNum;
extern INumberVectorProperty FocusTimerNP;
extern ISwitchVectorProperty ParkSP;
extern ISwitchVectorProperty PowerSP;
extern ISwitchVectorProperty FocusMotionSw;

static IText   VersionT[] ={{ "Date", "", 0, 0, 0, 0} ,
			   { "Time", "", 0, 0, 0, 0} ,
			   { "Number", "", 0, 0, 0 ,0} ,
			   { "Full", "", 0, 0, 0, 0} ,
			   { "Name", "" ,0 ,0 ,0 ,0}};

static ITextVectorProperty VersionInfo = {mydev, "Firmware Info", "", FirmwareGroup, IP_RO, 0, IPS_IDLE, VersionT, NARRAY(VersionT), "" ,0};

// Focus Control
static INumber	FocusSpeedN[]	 = {{"SPEED", "Speed", "%0.f", 0.0, 4.0, 1.0, 0.0, 0, 0, 0}};
static INumberVectorProperty	FocusSpeedNP  = {mydev, "FOCUS_SPEED", "Speed", FOCUS_GROUP, IP_RW, 0, IPS_IDLE, FocusSpeedN, NARRAY(FocusSpeedN), "", 0};

void changeLX200AutostarDeviceName(const char *newName)
{
  strcpy(VersionInfo.device, newName);
  strcpy(FocusSpeedNP.device, newName);
}

LX200Autostar::LX200Autostar() : LX200Generic()
{

}


void LX200Autostar::ISGetProperties (const char *dev)
{

if (dev && strcmp (thisDevice, dev))
    return;

    LX200Generic::ISGetProperties(dev);

    IDDefText   (&VersionInfo, NULL);
    IDDefNumber (&FocusSpeedNP, NULL);

    // For Autostar, we have a different focus speed method
    // Therefore, we don't need the classical one
    IDDelete(thisDevice, "FOCUS_MODE", NULL);

}

void LX200Autostar::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
        // ignore if not ours //
	if (strcmp (dev, thisDevice))
	    return;

	// suppress warning
	n=n;

  LX200Generic::ISNewText (dev, name, texts, names, n);

}


void LX200Autostar::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
        // ignore if not ours
	if (strcmp (dev, thisDevice))
	    return;

        // Focus speed
	if (!strcmp (name, FocusSpeedNP.name))
	{
	  if (checkPower(&FocusSpeedNP))
	   return;

	  if (IUUpdateNumbers(&FocusSpeedNP, values, names, n) < 0)
		return;

	  setGPSFocuserSpeed( ( (int) FocusSpeedN[0].value));
	  FocusSpeedNP.s = IPS_OK;
	  IDSetNumber(&FocusSpeedNP, NULL);
	  return;
	}

    LX200Generic::ISNewNumber (dev, name, values, names, n);
}

 void LX200Autostar::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
 {
   int index=0, err=0;
 
   if (!strcmp(name, ParkSP.name))
   {
	  if (checkPower(&ParkSP))
	    return;
           
	   ParkSP.s = IPS_IDLE;
	   
   	  if (eqNum.s == IPS_BUSY)
	  {
	     abortSlew();

	     // sleep for 200 mseconds
	     usleep(200000);
	  }

	  slewToPark();

	  ParkSP.s = IPS_OK;
	  eqNum.s = IPS_IDLE;
	  PowerSP.s   = IPS_IDLE;
	  PowerSP.sp[0].s = ISS_OFF;
	  PowerSP.sp[1].s = ISS_ON;
	  IDSetNumber(&eqNum, NULL);
	  IDSetSwitch(&ParkSP, "The telescope is slewing to park position. Turn off the telescope after park is complete. Disconnecting...");
	  IDSetSwitch(&PowerSP, NULL);
	  return;
    }

        // Focus Motion
	if (!strcmp (name, FocusMotionSw.name))
	{
	  if (checkPower(&FocusMotionSw))
	   return;

	  IUResetSwitches(&FocusMotionSw);
	  
	  // If speed is "halt"
	  if (FocusSpeedN[0].value == 0)
	  {
	    FocusMotionSw.s = IPS_IDLE;
	    IDSetSwitch(&FocusMotionSw, NULL);
	    return;
	  }
	  
	  IUUpdateSwitches(&FocusMotionSw, states, names, n);
	  index = getOnSwitch(&FocusMotionSw);
	  
	  
	  if ( ( err = setFocuserMotion(index) < 0) )
	  {
	     handleError(&FocusMotionSw, err, "Setting focuser speed");
             return;
	  }
	  
	  FocusMotionSw.s = IPS_BUSY;
	  
	  // with a timer 
	  if (FocusTimerNP.np[0].value > 0)  
	     FocusTimerNP.s  = IPS_BUSY;
	  
	  IDSetSwitch(&FocusMotionSw, NULL);
	  return;
	}

   LX200Generic::ISNewSwitch (dev, name, states, names,  n);

 }

 void LX200Autostar::ISPoll ()
 {

      LX200Generic::ISPoll();

 }

 void LX200Autostar::getBasicData()
 {

   VersionInfo.tp[0].text = new char[64];
   getVersionDate(VersionInfo.tp[0].text);
   VersionInfo.tp[1].text = new char[64];
   getVersionTime(VersionInfo.tp[1].text);
   VersionInfo.tp[2].text = new char[64];
   getVersionNumber(VersionInfo.tp[2].text);
   VersionInfo.tp[3].text = new char[128];
   getFullVersion(VersionInfo.tp[3].text);
   VersionInfo.tp[4].text = new char[128];
   getProductName(VersionInfo.tp[4].text);

   IDSetText(&VersionInfo, NULL);
   
   // process parent
   LX200Generic::getBasicData();

 }
