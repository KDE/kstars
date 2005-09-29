/*
    LX200 16"
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
#include <math.h>

#include "lx200_16.h"
#include "lx200driver.h"

#define LX16GROUP	"GPS/16 inch Features"

extern LX200Generic *telescope;
extern ITextVectorProperty Time;
extern int MaxReticleFlashRate;


//void turnFanOn();
   //void turnFanOff();

   //void seekHomeAndSave();
  // void seekHomeAndSet();

   /** Turns the field derotator On or Off. This command applies <b>only</b> to 16" LX200.
   @param turnOn if turnOn is true, the derotator is turned on, otherwise, it is turned off. */
 //  void fieldDeRotator(bool turnOn);


   /** Sets object Altitude. \n
   @returns true if object's altitude is successfully set. */
    //bool setObjAlt(int degrees, int minutes);

   /** Sets object Azimuth. \n
   @returns true if object's azimuth is successfully set. */
//    bool setObjAz(int degrees, int minutes);


static ISwitch FanStatusS[]		= { {"On", "", ISS_OFF, 0, 0}, {"Off", "", ISS_OFF, 0, 0}};
static ISwitch HomeSearchS[]		= { {"Save home", "", ISS_OFF, 0, 0} , {"Set home", "", ISS_OFF, 0, 0}};
static ISwitch FieldDeRotatorS[]	= { {"On", "", ISS_OFF, 0, 0}, {"Off", "", ISS_OFF,0 ,0}};
//static ISwitch SlewAltAzS[]		= { {"Slew To Alt/Az",  ISS_ON}};

#define	MAXINDINAME	32
#define	MAXINDILABEL	32
#define	MAXINDIDEVICE	32
#define	MAXINDIGROUP	32
#define	MAXINDIFORMAT	32

static ISwitchVectorProperty FanStatusSw	= { mydev, "Fan", "", LX16GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, FanStatusS, NARRAY(FanStatusS), "", 0};

static ISwitchVectorProperty HomeSearchSw	= { mydev, "Park", "", LX16GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, HomeSearchS, NARRAY(HomeSearchS), "", 0};

static ISwitchVectorProperty FieldDeRotatorSw	= { mydev, "Field De-rotator", "", LX16GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, FieldDeRotatorS, NARRAY(FieldDeRotatorS), "", 0};

//static ISwitches SlewAltAzSw		= { mydev, "AltAzSet", "On Alt/Az Set",  SlewAltAzS, NARRAY(SlewAltAzS), ILS_IDLE, 0, LX16Group};

/* horizontal position */
static INumber hor[] = {
    {"ALT",  "Alt  D:M:S", "%10.6m",  -90., 90., 0., 0., 0, 0, 0},
    {"AZ", "Az D:M:S", "%10.6m", 0., 360., 0., 0., 0, 0, 0}};
    
static INumberVectorProperty horNum = {
    mydev, "HORIZONTAL_COORD", "Horizontal Coords", LX16GROUP, IP_RW, 0, IPS_IDLE,
    hor, NARRAY(hor), "", 0};

void changeLX200_16DeviceName(const char * newName)
{
  strcpy(horNum.device, newName);
  strcpy(FanStatusSw.device, newName);
  strcpy(HomeSearchSw.device, newName);
  strcpy(FieldDeRotatorSw.device,newName);
}

LX200_16::LX200_16() : LX200Autostar()
{

}
 
void LX200_16::ISGetProperties (const char *dev)
{

if (dev && strcmp (thisDevice, dev))
    return;

  // process parent first
  LX200Autostar::ISGetProperties(dev);

  IDDefNumber (&horNum, NULL);

  IDDefSwitch (&FanStatusSw, NULL);
  IDDefSwitch (&HomeSearchSw, NULL);
  IDDefSwitch (&FieldDeRotatorSw, NULL);

}

void LX200_16::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{

	// ignore if not ours //
	if (strcmp (dev, thisDevice))
	    return;

        LX200Autostar::ISNewText (dev, name, texts, names,  n);

}

void LX200_16::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
  double newAlt=0, newAz=0;
  char altStr[64], azStr[64];
  int err;

  // ignore if not ours //
  if (strcmp (dev, thisDevice))
    return;

  if ( !strcmp (name, horNum.name) )
  {
      int i=0, nset=0;

      if (checkPower(&horNum))
	   return;

        for (nset = i = 0; i < n; i++)
	    {
		INumber *horp = IUFindNumber (&horNum, names[i]);
		if (horp == &hor[0])
		{
                    newAlt = values[i];
		    nset += newAlt >= -90. && newAlt <= 90.0;
		} else if (horp == &hor[1])
		{
		    newAz = values[i];
		    nset += newAz >= 0. && newAz <= 360.0;
		}
	    }

	  if (nset == 2)
	  {
	   if ( (err = setObjAz(newAz)) < 0 || (err = setObjAlt(newAlt)) < 0)
	   {
	     handleError(&horNum, err, "Setting Alt/Az");
	     return;
	   }
	        horNum.s = IPS_OK;
	       //horNum.n[0].value = values[0];
	       //horNum.n[1].value = values[1];
	       targetAz  = newAz;
	       targetAlt = newAlt;

	       fs_sexa(azStr, targetAz, 2, 3600);
	       fs_sexa(altStr, targetAlt, 2, 3600);

	       IDSetNumber (&horNum, "Attempting to slew to Alt %s - Az %s", altStr, azStr);
	       handleAltAzSlew();
	  }
	  else
	  {
		horNum.s = IPS_IDLE;
		IDSetNumber(&horNum, "Altitude or Azimuth missing or invalid");
	  }
	
	  return;  
    }

    LX200Autostar::ISNewNumber (dev, name, values, names, n);
}
    



void LX200_16::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
   int index;
   int err;

  if (strcmp (dev, thisDevice))
    return;

   if (!strcmp(name, FanStatusSw.name))
   {
      if (checkPower(&FanStatusSw))
       return;

          IUResetSwitches(&FanStatusSw);
          IUUpdateSwitches(&FanStatusSw, states, names, n);
          index = getOnSwitch(&FanStatusSw);

	  if (index == 0)
	  {
	    if ( (err = turnFanOn()) < 0)
	    {
	      handleError(&FanStatusSw, err, "Changing fan status");
	      return;
	    }
	  }
	  else
	  {
	    if ( (err = turnFanOff()) < 0)
	    {
	      handleError(&FanStatusSw, err, "Changing fan status");
	      return;
	    }
	  }
	  
	  FanStatusSw.s = IPS_OK;
	  IDSetSwitch (&FanStatusSw, index == 0 ? "Fan is ON" : "Fan is OFF");
	  return;
   }

   if (!strcmp(name, HomeSearchSw.name))
   {
      if (checkPower(&HomeSearchSw))
       return;

          IUResetSwitches(&HomeSearchSw);
          IUUpdateSwitches(&HomeSearchSw, states, names, n);
          index = getOnSwitch(&HomeSearchSw);

	  index == 0 ? seekHomeAndSave() : seekHomeAndSet();
	  HomeSearchSw.s = IPS_BUSY;
	  IDSetSwitch (&HomeSearchSw, index == 0 ? "Seek Home and Save" : "Seek Home and Set");
	  return;
   }

   if (!strcmp(name, FieldDeRotatorSw.name))
   {
      if (checkPower(&FieldDeRotatorSw))
       return;

          IUResetSwitches(&FieldDeRotatorSw);
          IUUpdateSwitches(&FieldDeRotatorSw, states, names, n);
          index = getOnSwitch(&FieldDeRotatorSw);

	  index == 0 ? seekHomeAndSave() : seekHomeAndSet();
	  FieldDeRotatorSw.s = IPS_OK;
	  IDSetSwitch (&FieldDeRotatorSw, index == 0 ? "Field deRotator is ON" : "Field deRotator is OFF");
	  return;
   }

    LX200Autostar::ISNewSwitch (dev, name, states, names, n);

}

void LX200_16::handleAltAzSlew()
{
        int i=0;
	char altStr[64], azStr[64];

	  if (horNum.s == IPS_BUSY)
	  {
	     abortSlew();

	     // sleep for 100 mseconds
	     usleep(100000);
	  }

	  if ((i = slewToAltAz()))
	  {
	    horNum.s = IPS_IDLE;
	    IDSetNumber(&horNum, "Slew not possible");
	    return;
	  }

	  horNum.s = IPS_BUSY;
	  fs_sexa(azStr, targetAz, 2, 3600);
	  fs_sexa(altStr, targetAlt, 2, 3600);

	  IDSetNumber(&horNum, "Slewing to Alt %s - Az %s", altStr, azStr);
	  return;
}

 void LX200_16::ISPoll ()
 {
   int searchResult=0;
   double dx, dy;
   int err;
   
   LX200Autostar::ISPoll();

   	switch (HomeSearchSw.s)
	{
	case IPS_IDLE:
	     break;

	case IPS_BUSY:

	    if ( (err = getHomeSearchStatus(&searchResult)) < 0)
	    {
	      handleError(&HomeSearchSw, err, "Home search");
	      return;
	    }

	    if (searchResult == 0)
	    {
	      HomeSearchSw.s = IPS_IDLE;
	      IDSetSwitch(&HomeSearchSw, "Home search failed.");
	    }
	    else if (searchResult == 1)
	    {
	      HomeSearchSw.s = IPS_OK;
	      IDSetSwitch(&HomeSearchSw, "Home search successful.");
	    }
	    else if (searchResult == 2)
	      IDSetSwitch(&HomeSearchSw, "Home search in progress...");
	    else
	    {
	      HomeSearchSw.s = IPS_IDLE;
	      IDSetSwitch(&HomeSearchSw, "Home search error.");
	    }
	    break;

	 case IPS_OK:
	   break;
	 case IPS_ALERT:
	  break;
	}

	switch (horNum.s)
	{
	case IPS_IDLE:
	     break;

	case IPS_BUSY:

	    if ( (err = getLX200Az(&currentAz)) < 0 || (err = getLX200Alt(&currentAlt)) < 0)
	    {
	      IDSetNumber(&horNum, NULL);
	      handleError(&horNum, err, "Get Alt/Az");
	      return;
	    }
	    
	    dx = targetAz - currentAz;
	    dy = targetAlt - currentAlt;

            horNum.np[0].value = currentAlt;
	    horNum.np[1].value = currentAz;

	    IDLog("targetAz is %g, currentAz is %g\n", targetAz, currentAz);
	    IDLog("targetAlt is %g, currentAlt is %g\n**********************\n", targetAlt,  currentAlt);


	    // accuracy threshhold (3'), can be changed as desired.
	    if ( fabs(dx) <= 0.05 && fabs(dy) <= 0.05)
	    {

		horNum.s = IPS_OK;
		currentAz = targetAz;
		currentAlt = targetAlt;
                IDSetNumber (&horNum, "Slew is complete");
	    } else
	    	IDSetNumber (&horNum, NULL);
	    break;

	case IPS_OK:
	    break;

	case IPS_ALERT:
	    break;
	}

 }

 void LX200_16::getBasicData()
 {

   getLX200Az(&targetAz);
   getLX200Alt(&targetAlt);

   horNum.np[0].value = targetAlt;
   horNum.np[1].value = targetAz;

   IDSetNumber (&horNum, NULL);
   
   LX200Autostar::getBasicData();

 }
