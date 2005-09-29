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


extern LX200Generic *telescope;
extern int MaxReticleFlashRate;
extern ITextVectorProperty Time;
extern INumberVectorProperty SDTime;
extern INumberVectorProperty eqNum;
extern ISwitchVectorProperty ParkSP;
extern ISwitchVectorProperty PowerSP;

static IText   VersionT[] ={{ "Date", "", 0, 0, 0, 0} ,
			   { "Time", "", 0, 0, 0, 0} ,
			   { "Number", "", 0, 0, 0 ,0} ,
			   { "Full", "", 0, 0, 0, 0} ,
			   { "Name", "" ,0 ,0 ,0 ,0}};

static ITextVectorProperty VersionInfo = {mydev, "Firmware Info", "", FirmwareGroup, IP_RO, 0, IPS_IDLE, VersionT, NARRAY(VersionT), "" ,0};

void changeLX200AutostarDeviceName(const char *newName)
{
  strcpy(VersionInfo.device, newName);
}

LX200Autostar::LX200Autostar() : LX200Generic()
{

  //for (int i=0; i  < 5; i++)
     //strcpy(VersionInfo.t[i].text, "");

}


void LX200Autostar::ISGetProperties (const char *dev)
{

if (dev && strcmp (thisDevice, dev))
    return;

    LX200Generic::ISGetProperties(dev);

    IDDefText (&VersionInfo, NULL);

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
    LX200Generic::ISNewNumber (dev, name, values, names, n);
}

 void LX200Autostar::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
 {
 
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
