/*
    LX200 GPS
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

#include "lx200gps.h"
#include "lx200driver.h"

#define GPSGroup   "Extended GPS Features"

extern LX200Generic *telescope;
extern char mydev[];
extern ITextVectorProperty Time;
extern int MaxReticleFlashRate;

static ISwitch GPSPowerS[]		= {{ "On", "", ISS_OFF}, {"Off", "", ISS_ON}};
static ISwitch GPSStatusS[]	  	= {{ "GPS Sleep", "", ISS_OFF}, {"GPS Wake up", "", ISS_OFF}, {"GPS Restart", "", ISS_OFF}};
static ISwitch GPSUpdateS[]	  	= { {"Update GPS system", "", ISS_OFF}};
static ISwitch AltDecPecS[]		= {{ "Enable", "", ISS_OFF}, {"Disable", "", ISS_OFF}};
static ISwitch AzRaPecS[]		= {{ "Enable", "", ISS_OFF}, {"Disable", "", ISS_OFF}};
static ISwitch SelenSyncS[]		= {{ "Selenographic Sync", "",  ISS_OFF}};
static ISwitch AltDecBackSlashS[]	= {{ "Activate Alt Dec Anti Backslash", "", ISS_OFF}};
static ISwitch AzRaBackSlashS[]		= {{ "Activate Az Ra Anti Backslash", "", ISS_OFF}};

static ISwitchVectorProperty GPSPowerSw	   = { mydev, "GPS Power", "", GPSGroup, IP_RW, ISR_1OFMANY, 0 , IPS_IDLE, GPSPowerS, NARRAY(GPSPowerS)};
static ISwitchVectorProperty GPSStatusSw   = { mydev, "GPS Status", "", GPSGroup, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, GPSStatusS, NARRAY(GPSStatusS)};
static ISwitchVectorProperty GPSUpdateSw   = { mydev, "GPS Update", "", GPSGroup, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, GPSUpdateS, NARRAY(GPSUpdateS)};
static ISwitchVectorProperty AltDecPecSw   = { mydev, "Alt/Dec PEC Compensation", "", GPSGroup, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, AltDecPecS, NARRAY(AltDecPecS)};
static ISwitchVectorProperty AzRaPecSw	   = { mydev, "Az/Ra PEC Compensation", "", GPSGroup, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, AzRaPecS, NARRAY(AzRaPecS)};
static ISwitchVectorProperty SelenSyncSw   = { mydev, "Selenographic Sync", "", GPSGroup, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, SelenSyncS, NARRAY(SelenSyncS)};
static ISwitchVectorProperty AltDecBackSlashSw	= { mydev, "Alt/Dec Anti-backslash", "", GPSGroup, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, AltDecBackSlashS, NARRAY(AltDecBackSlashS)};
static ISwitchVectorProperty AzRaBackSlashSw	= { mydev, "Az/Ra Anti-backslash", "", GPSGroup, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, AzRaBackSlashS, NARRAY(AzRaBackSlashS)};

static INumber Temp[]	= { {"Temp.", "", "%g", 0., 0., 0., 0. } };
static INumberVectorProperty OTATemp =   { mydev, "OTA Temperature, degrees C", "", GPSGroup, IP_RO, 0, IPS_IDLE, Temp, NARRAY(Temp)};

LX200GPS::LX200GPS() : LX200_16()
{

}

void LX200GPS::ISGetProperties (const char *dev)
{

if (dev && strcmp (mydev, dev))
    return;

// process parent first
   LX200_16::ISGetProperties(dev);

IDDefSwitch (&GPSPowerSw);
IDDefSwitch (&GPSStatusSw);
IDDefSwitch (&GPSUpdateSw);
IDDefSwitch (&AltDecPecSw);
IDDefSwitch (&AzRaPecSw);
IDDefSwitch (&SelenSyncSw);
IDDefSwitch (&AltDecBackSlashSw);
IDDefSwitch (&AzRaBackSlashSw);
IDDefNumber (&OTATemp);


}

void LX200GPS::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
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

	   IDLog("*** We are in the AUTOSTAR time update ***\n");

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

		IDLog("The initial UTC offset is %g\n", UTCOffset);

		if (utm.tm_mday - ltp->tm_mday != 0)
			 UTCOffset += 24;

		if (ltp->tm_isdst > 0)
		{
		  UTCOffset++;
		  IDLog("Correcting for DST, new UTC is %g\n", UTCOffset);
		}

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
		  IDSetText(&Time , "Time updated to %s. Current Autostar UTC is %g", texts[0], UTCOffset*-1);
		  return;
		}

		localTM = ltp;
		setCalenderDate(ltp->tm_mday, ltp->tm_mon, ltp->tm_year);
 		IDSetText(&Time , "Date changed, updating planetary data...");

		return;
	}

     LX200_16::ISNewText (dev, name, texts, names, n);
}

void LX200GPS::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    LX200_16::ISNewNumber (dev, name, values, names, n);

 }

 /*#define turnGPSOn()			portWrite("#:g+#")
#define turnGPSOff()			portWrite("#:g-#")
#define alignGPSScope()			portWrite("#:Aa#")
#define gpsSleep()			portWrite("#:hN#")
#define gpsWakeUp()			portWrite("#:hW#")
#define gpsRestart()			portWrite("#:I#")
#define updateGPS_System()		setStandardProcedure("#:gT#")
#define enableDecAltPec()		portWrite("#:QA+#")
#define disableDecAltPec()		portWrite("#:QA-#")
#define enableRA_AZPec()		portWrite("#:QZ+#")
#define disableRA_AZPec()		portWrite("#:QZ-#")
#define activateAltDecAntiBackSlash()	portWrite("#$BAdd#")
#define activateAzRaAntiBackSlash()	portWrite("#$BZdd#")
#define SelenographicSync()		portWrite("#:CL#")

double getOTATemp();
*/


 void LX200GPS::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
 {
    int index;
    char msg[32];

    LX200_16::ISNewSwitch (dev, name, states, names,  n);

    if (strcmp (dev, mydev))
	    return;

    if (!strcmp(name,GPSPowerSw.name))
    {
      if (checkPower(&GPSPowerSw))
       return;

      index = getOnSwitch(states, n);
      resetSwitches(&GPSPowerSw);
      index == 0 ? turnGPSOn() : turnGPSOff();
      GPSPowerSw.sw[index].s = ISS_ON;
      GPSPowerSw.s = IPS_OK;
      strcpy(msg, index == 0 ? "GPS System is ON" : "GPS System is OFF");
      IDSetSwitch (&GPSPowerSw, msg);
      return;
    }

    if (!strcmp(name,GPSStatusSw.name))
    {
      if (checkPower(&GPSStatusSw))
       return;

      index = getOnSwitch(states, n);
      resetSwitches(&GPSPowerSw);

      if (index == 0)
      {
	   gpsSleep();
	   strcpy(msg, "GPS system is in sleep mode");
      }
      else if (index == 1)
      {
	   gpsWakeUp();
           strcpy(msg, "GPS system is reactivated");
      }
      else
      {
	   gpsRestart();
	   strcpy(msg, "GPS system is restarting...");
      }

	GPSStatusSw.s = IPS_OK;
        GPSStatusSw.sw[index].s = ISS_ON;
	IDSetSwitch (&GPSStatusSw, msg);
	return;

    }

    if (!strcmp(name,GPSUpdateSw.name))
    {
      if (checkPower(&GPSUpdateSw))
       return;

     GPSUpdateSw.s = IPS_OK;
     IDSetSwitch(&GPSUpdateSw, "Updating GPS system. This operation might take few minutes to complete...");
     if (updateGPS_System())
     	IDSetSwitch(&GPSUpdateSw, "GPS system update successful");
     else
     {
        GPSUpdateSw.s = IPS_IDLE;
        IDSetSwitch(&GPSUpdateSw, "GPS system update failed");
     }
     return;
    }

    if (!strcmp(name, AltDecPecSw.name))
    {
      if (checkPower(&AltDecPecSw))
       return;

      index = getOnSwitch(states, n);
      resetSwitches(&AltDecPecSw);

       if (index == 0)
      {
        enableDecAltPec();
	strcpy (msg, "Alt/Dec Compensation Enabled");
      }
      else
      {
        disableDecAltPec();
	strcpy (msg, "Alt/Dec Compensation Disabled");
      }

      AltDecPecSw.s = IPS_OK;
      AltDecPecSw.sw[index].s = ISS_ON;
      IDSetSwitch(&AltDecPecSw, msg);

      return;
    }

    if (!strcmp(name, AzRaPecSw.name))
    {
      if (checkPower(&AzRaPecSw))
       return;

      index = getOnSwitch(states, n);
      resetSwitches(&AzRaPecSw);

       if (index == 0)
      {
        enableRaAzPec();
	strcpy (msg, "Ra/Az Compensation Enabled");
      }
      else
      {
        disableRaAzPec();
	strcpy (msg, "Ra/Az Compensation Disabled");
      }

      AzRaPecSw.s = IPS_OK;
      AzRaPecSw.sw[index].s = ISS_ON;
      IDSetSwitch(&AzRaPecSw, msg);

      return;
    }

   if (!strcmp(name, AltDecBackSlashSw.name))
   {
     if (checkPower(&AltDecBackSlashSw))
      return;

     activateAltDecAntiBackSlash();
     AltDecBackSlashSw.s = IPS_OK;
     IDSetSwitch(&AltDecBackSlashSw, "Alt/Dec Anti-backslash enabled");
     return;
   }

   if (!strcmp(name, AzRaBackSlashSw.name))
   {
     if (checkPower(&AzRaBackSlashSw))
      return;

     activateAzRaAntiBackSlash();
     AzRaBackSlashSw.s = IPS_OK;
     IDSetSwitch(&AzRaBackSlashSw, "Az/Ra Anti-backslash enabled");
     return;
   }

}

 void LX200GPS::ISPoll ()
 {

   LX200_16::ISPoll();

   if (isTelescopeOn())
   {
   	OTATemp.n[0].value = getOTATemp();
   	IDSetNumber(&OTATemp, NULL);
   }

 }

 void LX200GPS::getBasicData()
 {

   // process parent first
   LX200_16::getBasicData();

   OTATemp.n[0].value = getOTATemp();
   IDSetNumber(&OTATemp, NULL);
 }

