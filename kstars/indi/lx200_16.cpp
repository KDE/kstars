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
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

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
extern char mydev[];
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


static ISwitch FanStatusS[]		= { {"On", "", ISS_OFF}, {"Off", "", ISS_OFF}};
static ISwitch HomeSearchS[]		= { {"Seek home and save", "", ISS_OFF} , {"Seek home and set", "", ISS_OFF}};
static ISwitch FieldDeRotatorS[]	= { {"On", "", ISS_OFF}, {"Off", "", ISS_OFF}};
//static ISwitch SlewAltAzS[]		= { {"Slew To Alt/Az",  ISS_ON}};

static ISwitchVectorProperty FanStatusSw	= { mydev, "Fan", "", LX16GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, FanStatusS, NARRAY(FanStatusS)};

static ISwitchVectorProperty HomeSearchSw	= { mydev, "Park", "", LX16GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, HomeSearchS, NARRAY(HomeSearchS)};

static ISwitchVectorProperty FieldDeRotatorSw	= { mydev, "Field De-rotator", "", LX16GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, FieldDeRotatorS, NARRAY(FieldDeRotatorS)};

//static ISwitches SlewAltAzSw		= { mydev, "AltAzSet", "On Alt/Az Set",  SlewAltAzS, NARRAY(SlewAltAzS), ILS_IDLE, 0, LX16Group};

/* horizontal position */
static INumber hor[] = {
    {"ALT",  "Alt  D:M:S", "%10.6m",  -90., 90., 0., 0.},
    {"AZ", "Az D:M:S", "%10.6m", 0., 360., 0., 0.},
};
static INumberVectorProperty horNum = {
    mydev, "HORIZONTAL_COORD", "Horizontal Coords", LX16GROUP, IP_RW, 0, IPS_IDLE,
    hor, NARRAY(hor),
};

LX200_16::LX200_16() : LX200Autostar()
{

}

void LX200_16::ISGetProperties (const char *dev)
{

if (dev && strcmp (mydev, dev))
    return;

  // process parent first
  LX200Autostar::ISGetProperties(dev);

  IDDefNumber (&horNum);

  IDDefSwitch (&FanStatusSw);
  IDDefSwitch (&HomeSearchSw);
  IDDefSwitch (&FieldDeRotatorSw);

}

void LX200_16::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{

        double UTCOffset;
	struct tm *ltp = new tm;
	struct tm utm;
	time_t ltime;
	time (&ltime);
	localtime_r (&ltime, ltp);
	IText *tp;

	// ignore if not ours //
	if (strcmp (dev, mydev))
	    return;

	// suppress warning
	n=n;

       // Override LX200 Autostar
       if (!strcmp (name, Time.name))
       {
	  if (checkPower(&Time))
	   return;

	  if (extractISOTime(texts[0], &utm) < 0)
	  {
	    Time.s = IPS_IDLE;
	    IDSetText(&Time , "Time invalid");
	    return;
	  }
	        utm.tm_mon   += 1;
		ltp->tm_mon  += 1;
		utm.tm_year  += 1900;
		ltp->tm_year += 1900;

	  	UTCOffset = (utm.tm_hour - ltp->tm_hour);

		if (utm.tm_mday - ltp->tm_mday != 0)
			 UTCOffset += 24;

		IDLog("time is %02d:%02d:%02d\n", ltp->tm_hour, ltp->tm_min, ltp->tm_sec);
		setUTCOffset(UTCOffset);
	  	setLocalTime(ltp->tm_hour, ltp->tm_min, ltp->tm_sec);

		tp = IUFindText(&Time, names[0]);
		if (!tp)
		 return;
		tp->text = new char[strlen(texts[0]+1)];
	        strcpy(tp->text, texts[0]);
		Time.s = IPS_OK;

		// update JD
                JD = UTtoJD(&utm);

		IDLog("New JD is %f\n", (float) JD);

		if ((localTM->tm_mday == ltp->tm_mday ) && (localTM->tm_mon == ltp->tm_mon) &&
		    (localTM->tm_year == ltp->tm_year))
		{
		  IDSetText(&Time , "Time updated to %s", texts[0]);
		  return;
		}

		localTM = ltp;
		setCalenderDate(ltp->tm_mday, ltp->tm_mon, ltp->tm_year);
 		IDSetText(&Time , "Date changed, updating planetary data...");

		return;
	}


   LX200Autostar::ISNewText (dev, name, texts, names,  n);

}

void LX200_16::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
  double newAlt=0, newAz=0;
  char altStr[64], azStr[64];


  LX200Autostar::ISNewNumber (dev, name, values, names, n);

  // ignore if not ours //
  if (strcmp (dev, mydev))
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
	   if (!setObjAz(newAz) && !setObjAlt(newAlt))
	   {

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
	        horNum.s = IPS_ALERT;
		IDSetNumber (&horNum, "Error setting coordinates.");
            }

	    return;

	   } // end nset
	   else
	   {
		horNum.s = IPS_IDLE;
		IDSetNumber(&horNum, "Altitude or Azimuth missing or invalid");
	   }

    }



 }


void LX200_16::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
   int index;
   char msg[32];

  if (strcmp (dev, mydev))
    return;

   // process parent
   LX200Autostar::ISNewSwitch (dev, name, states, names, n);

   if (!strcmp(name, FanStatusSw.name))
   {
      if (checkPower(&FanStatusSw))
       return;

          index = getOnSwitch(states, n);
	  resetSwitches(&FanStatusSw);

	  index == 0 ? turnFanOn() : turnFanOff();
	  FanStatusSw.s = IPS_OK;
	  strcpy(msg, index == 0 ? "Fan is ON" : "Fan is OFF");
	  IDSetSwitch (&FanStatusSw, msg);
	  return;
   }

   if (!strcmp(name, HomeSearchSw.name))
   {
      if (checkPower(&HomeSearchSw))
       return;

          index = getOnSwitch(states, n);
	  resetSwitches(&HomeSearchSw);

	  index == 0 ? seekHomeAndSave() : seekHomeAndSet();
	  HomeSearchSw.s = IPS_BUSY;
	  IDSetSwitch (&HomeSearchSw, msg);
	  return;
   }

   if (!strcmp(name, FieldDeRotatorSw.name))
   {
      if (checkPower(&FieldDeRotatorSw))
       return;

          index = getOnSwitch(states, n);
	  resetSwitches(&FieldDeRotatorSw);

	  index == 0 ? seekHomeAndSave() : seekHomeAndSet();
	  FieldDeRotatorSw.s = IPS_OK;
	  strcpy(msg, index == 0 ? "Field deRotator is ON" : "Field deRotator is OFF");
	  IDSetSwitch (&FieldDeRotatorSw, msg);
	  return;
   }


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
   int searchResult;
   double dx, dy;
   LX200Autostar::ISPoll();

   	switch (HomeSearchSw.s)
	{
	case IPS_IDLE:
	     break;

	case IPS_BUSY:

	    searchResult = getHomeSearchStatus();

	    if (!searchResult)
	    {
	      HomeSearchSw.s = IPS_IDLE;
	      IDSetSwitch(&HomeSearchSw, "Home search failed");
	    }
	    else if (searchResult == 1)
	    {
	      HomeSearchSw.s = IPS_OK;
	      IDSetSwitch(&HomeSearchSw, "Home search successful");
	    }
	    else if (searchResult == 2)
	      IDSetSwitch(&HomeSearchSw, "Home search in progress...");
	    else
	    {
	      HomeSearchSw.s = IPS_IDLE;
	      IDSetSwitch(&HomeSearchSw, "Home search error");
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

	    currentAz = getLX200Az();
	    currentAlt = getLX200Alt();
	    dx = targetAz - currentAz;
	    dy = targetAlt - currentAlt;

            horNum.n[0].value = currentAlt;
	    horNum.n[1].value = currentAz;

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
   // process parent first
   LX200Autostar::getBasicData();

   targetAz = getLX200Az();
   targetAlt = getLX200Alt();

   horNum.n[0].value = targetAlt;
   horNum.n[1].value = targetAz;

   IDSetNumber (&horNum, NULL);

 }
