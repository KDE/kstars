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
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

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
extern char mydev[];

static IText   VersionT[] ={{ "Version Date", "", ""} ,
			   { "Version Time", "", ""} ,
			   { "Version Number", "", ""} ,
			   { "Full Version", "", ""} ,
			   { "Product Name", "", ""}};

static ITextVectorProperty VersionInfo = {mydev, "Firmware Info", "", FirmwareGroup, IP_RO, 0, IPS_IDLE, VersionT, NARRAY(VersionT)};

LX200Autostar::LX200Autostar() : LX200Generic()
{

  //for (int i=0; i  < 5; i++)
     //strcpy(VersionInfo.t[i].text, "");

}


void LX200Autostar::ISGetProperties (const char *dev)
{

if (dev && strcmp (mydev, dev))
    return;

    LX200Generic::ISGetProperties(dev);

    IDDefText (&VersionInfo);

}

void LX200Autostar::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
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

       // Override LX200 Generic
       if (!strcmp (name, Time.name))
       {
	  if (checkPower(&Time))
	   return;

	   int autostarUTC;
	   int UTCDiff;

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

	  	UTCOffset = (ltp->tm_hour - utm.tm_hour);

		if (utm.tm_mday - ltp->tm_mday != 0)
			 UTCOffset -= 24;

		IDLog("time is %02d:%02d:%02d\n", ltp->tm_hour, ltp->tm_min, ltp->tm_sec);
		setUTCOffset(UTCOffset);

		// wait for autostar a bit and then read what UTC it sets
		// The autostar UTC is affected by Day Light Saving option which
		// we cannot inquire directly.
		usleep(100000);

		autostarUTC = getUTCOffset();
		UTCDiff = (int) UTCOffset - autostarUTC;

		// The UTC we send the autostar already accounts for day light savings
		// assuming the machine time is correct. If autostar incoperates day light savings
		// we need to reverse that process
		if (UTCDiff)
		  setUTCOffset(UTCOffset - UTCDiff);

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

  LX200Generic::ISNewText (dev, name, texts, names, n);

}


void LX200Autostar::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    LX200Generic::ISNewNumber (dev, name, values, names, n);
}

 void LX200Autostar::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
 {

   LX200Generic::ISNewSwitch (dev, name, states, names,  n);

 }

 void LX200Autostar::ISPoll ()
 {

      LX200Generic::ISPoll();

 }

 void LX200Autostar::getBasicData()
 {

   // process parent first
   LX200Generic::getBasicData();

   VersionInfo.t[0].text = new char[64];
   getVersionDate(VersionInfo.t[0].text);
   VersionInfo.t[1].text = new char[64];
   getVersionTime(VersionInfo.t[1].text);
   VersionInfo.t[2].text = new char[64];
   getVersionNumber(VersionInfo.t[2].text);
   VersionInfo.t[3].text = new char[128];
   getFullVersion(VersionInfo.t[3].text);
   VersionInfo.t[4].text = new char[128];
   getProductName(VersionInfo.t[4].text);

   IDSetText(&VersionInfo, NULL);


 }
