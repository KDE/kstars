#if 0
    LX200 Generic
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

#include "indicom.h"
#include "lx200driver.h"
#include "lx200gps.h"
#include "lx200classic.h"


LX200Generic *telescope = NULL;
int MaxReticleFlashRate = 3;

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
#define DATETIME_GROUP	"Date/Time"
#define SITE_GROUP	"Site Management"
#define FOCUS_GROUP	"Focus Control"

#define RA_THRESHOLD	0.01
#define DEC_THRESHOLD	0.05
#define LX200_SLEW	0
#define LX200_TRACK	1
#define LX200_SYNC	2
#define LX200_PARK	3

static void ISPoll(void *);
static void retryConnection(void *);

/*INDI controls */
static ISwitch PowerS[]          = {{"CONNECT" , "Connect" , ISS_OFF},{"DISCONNECT", "Disconnect", ISS_ON}};
static ISwitch AlignmentS []     = {{"Polar", "", ISS_ON}, {"AltAz", "", ISS_OFF}, {"Land", "", ISS_OFF}};
static ISwitch SitesS[]          = {{"Site 1", "", ISS_ON}, {"Site 2", "", ISS_OFF},  {"Site 3", "", ISS_OFF},  {"Site 4", "", ISS_OFF}};
static ISwitch SlewModeS[]       = {{"Max", "", ISS_ON}, {"Find", "", ISS_OFF}, {"Centering", "", ISS_OFF}, {"Guide", "", ISS_OFF}};
static ISwitch OnCoordSetS[]     = {{"SLEW", "Slew", ISS_ON }, {"TRACK", "Track", ISS_OFF}, {"SYNC", "Sync", ISS_OFF }};
static ISwitch TrackModeS[]      = {{ "Default", "", ISS_ON} , { "Lunar", "", ISS_OFF}, {"Manual", "", ISS_OFF}};
static ISwitch abortSlewS[]      = {{"ABORT", "Abort", ISS_OFF }};
static ISwitch ParkS[]		 = { {"Park", "", ISS_OFF} };

static ISwitch MovementS[]       = {{"N", "North", ISS_OFF}, {"W", "West", ISS_OFF}, {"E", "East", ISS_OFF}, {"S", "South", ISS_OFF}};

static ISwitch	FocusSpeedS[]	 = { {"Halt", "", ISS_ON}, { "Fast", "", ISS_OFF }, {"Slow", "", ISS_OFF}};
static ISwitch  FocusMotionS[]	 = { {"Focus in", "", ISS_OFF}, {"Focus out", "", ISS_OFF}};

/* equatorial position */
INumber eq[] = {
    {"RA",  "RA  H:M:S", "%10.6m",  0., 24., 0., 0.},
    {"DEC", "Dec D:M:S", "%10.6m", -90., 90., 0., 0.},
};
//TODO decide appropiate TIME_OUT
// N.B. No Static identifier as it is needed for external linkage
INumberVectorProperty eqNum = {
    mydev, "EQUATORIAL_COORD", "Equatorial J2000", BASIC_GROUP, IP_RW, 0, IPS_IDLE,
    eq, NARRAY(eq),
};

/* Fundamental group */
ISwitchVectorProperty PowerSP		= { mydev, "CONNECTION" , "Connection", COMM_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, PowerS, NARRAY(PowerS)};
static IText PortT[]			= {{"PORT", "Port", "/dev/ttyS0"}};
static ITextVectorProperty Port		= { mydev, "DEVICE_PORT", "Ports", COMM_GROUP, IP_RW, 0, IPS_IDLE, PortT, NARRAY(PortT)};

/* Basic data group */
static ISwitchVectorProperty AlignmentSw	= { mydev, "Alignment", "", COMM_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, AlignmentS, NARRAY(AlignmentS)};

static INumber altLimit[] = {
       {"minAlt", "min Alt", "%+03f", -90., 90., 0., 0.},
       {"maxAlt", "max Alt", "%+03f", -90., 90., 0., 0.}};
static INumberVectorProperty elevationLimit = { mydev, "altLimit", "Slew elevation Limit", BASIC_GROUP, IP_RW, 0, IPS_IDLE, altLimit, NARRAY(altLimit)};

/* Movement group */
static ISwitchVectorProperty OnCoordSetSw    = { mydev, "ON_COORD_SET", "On Set", BASIC_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, OnCoordSetS, NARRAY(OnCoordSetS)};

static ISwitchVectorProperty abortSlewSw     = { mydev, "ABORT_MOTION", "Abort Slew/Track", BASIC_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, abortSlewS, NARRAY(abortSlewS)};

ISwitchVectorProperty	ParkSP = {mydev, "PARK", "Park Scope", BASIC_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, ParkS, NARRAY(ParkS) };

static ISwitchVectorProperty SlewModeSw      = { mydev, "Slew rate", "", MOVE_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, SlewModeS, NARRAY(SlewModeS)};

static ISwitchVectorProperty TrackModeSw  = { mydev, "Tracking Mode", "", MOVE_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, TrackModeS, NARRAY(TrackModeS)};

static INumber TrackFreq[]  = {{ "trackFreq", "Freq", "%g", 50.0, 80.0, 0.1, 60.1}};

static INumberVectorProperty TrackingFreq= { mydev, "Tracking Frequency", "", MOVE_GROUP, IP_RW, 0, IPS_IDLE, TrackFreq, NARRAY(TrackFreq)};

static ISwitchVectorProperty MovementSw      = { mydev, "MOVEMENT", "Move toward", MOVE_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, MovementS, NARRAY(MovementS)};

// Focus Control
static ISwitchVectorProperty	FocusSpeedSw  = {mydev, "Speed", "", FOCUS_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, FocusSpeedS, NARRAY(FocusSpeedS)};

static ISwitchVectorProperty	FocusMotionSw = {mydev, "Motion", "", FOCUS_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, FocusMotionS, NARRAY(FocusMotionS)};

/* Data & Time */
static IText UTC[] = {{"UTC", "UTC", "YYYY-MM-DDTHH:MM:SS"}};
ITextVectorProperty Time = { mydev, "TIME", "UTC Time", DATETIME_GROUP, IP_RW, 0, IPS_IDLE, UTC, NARRAY(UTC)};
static INumber STime[] = {{"LST", "Sidereal time", "%10.6m" , 0.,0.,0.,0.}};
INumberVectorProperty SDTime = { mydev, "SDTIME", "Sidereal Time", DATETIME_GROUP, IP_RW, 0, IPS_IDLE, STime, NARRAY(STime)};


/* Site managment */
static ISwitchVectorProperty SitesSw  = { mydev, "Sites", "", SITE_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, SitesS, NARRAY(SitesS)};
/* geographic location */
static INumber geo[] = {
    {"LAT",  "Lat.  D:M:S +N", "%10.6m",  -90.,  90., 0., 0.},
    {"LONG", "Long. D:M:S +E", "%10.6m", 0., 360., 0., 0.},
};
static INumberVectorProperty geoNum = {
    mydev, "GEOGRAPHIC_COORD", "Geographic Location", SITE_GROUP, IP_RW, 0., IPS_IDLE,
    geo, NARRAY(geo),
};
static IText   SiteNameT[] = {{"SiteName", "", ""}};
static ITextVectorProperty SiteName = { mydev, "Site Name", "", SITE_GROUP, IP_RW, 0 , IPS_IDLE, SiteNameT, NARRAY(SiteNameT)};

void changeLX200GenericDeviceName(char * newName)
{
  strcpy(PowerSP.device , newName);
  strcpy(Port.device , newName);
  strcpy(AlignmentSw.device, newName);

  // BASIC_GROUP
  strcpy(elevationLimit.device , newName );
  strcpy(eqNum.device, newName);
  strcpy(OnCoordSetSw.device , newName );
  strcpy(abortSlewSw.device , newName );
  strcpy(ParkSP.device, newName);

  // MOVE_GROUP
  strcpy(SlewModeSw.device , newName );
  strcpy(TrackModeSw.device , newName );
  strcpy(TrackingFreq.device , newName );
  strcpy(MovementSw.device , newName );

  // FOCUS_GROUP
  strcpy(FocusSpeedSw.device , newName );
  strcpy(FocusMotionSw.device , newName );

  // DATETIME_GROUP
  strcpy(Time.device , newName );
  strcpy(SDTime.device , newName );

  // SITE_GROUP
  strcpy(SitesSw.device , newName );
  strcpy(SiteName.device , newName );
  strcpy(geoNum.device , newName );
  
}

void changeAllDeviceNames(char *newName)
{
  changeLX200GenericDeviceName(newName);
  changeLX200AutostarDeviceName(newName);
  changeLX200_16DeviceName(newName);
  changeLX200ClassicDeviceName(newName);
  changeLX200GPSDeviceName(newName);
}


/* send client definitions of all properties */
void ISInit()
{
  static int isInit=0;

 if (isInit)
  return;

 isInit = 1;

 
  if (strstr(me, "lx200classic"))
  {
     fprintf(stderr , "initilizaing from LX200 classic device...\n");
     // 1. mydev = device_name
     changeAllDeviceNames("LX200 Classic");
     // 2. device = sub_class
     telescope = new LX200Classic();
     telescope->setCurrectDeviceName("LX200 Classic");

     MaxReticleFlashRate = 3;
  }

  else if (strstr(me, "lx200gps"))
  {
     fprintf(stderr , "initilizaing from LX200 GPS device...\n");
     // 1. mydev = device_name
     changeAllDeviceNames("LX200 GPS");
     // 2. device = sub_class
     telescope = new LX200GPS();
     telescope->setCurrectDeviceName("LX200 GPS");

     MaxReticleFlashRate = 9;
  }
  else if (strstr(me, "lx200_16"))
  {

    IDLog("Initilizaing from LX200 16 device...\n");
    // 1. mydev = device_name
    changeAllDeviceNames("LX200 16");
    // 2. device = sub_class
   telescope = new LX200_16();
   telescope->setCurrectDeviceName("LX200 16");

   MaxReticleFlashRate = 3;
 }
 else if (strstr(me, "lx200autostar"))
 {
   fprintf(stderr , "initilizaing from autostar device...\n");
  
   // 1. change device name
   changeAllDeviceNames("LX200 Autostar");
   // 2. device = sub_class
   telescope = new LX200Autostar();
   telescope->setCurrectDeviceName("LX200 Autostar");

   MaxReticleFlashRate = 9;
 }
 // be nice and give them a generic device
 else
 {
  telescope = new LX200Generic();
  telescope->setCurrectDeviceName("LX200 Generic");
 }

}

void ISGetProperties (const char *dev)
{ ISInit(); telescope->ISGetProperties(dev); IEAddTimer (POLLMS, ISPoll, NULL);}
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

LX200Generic::LX200Generic()
{
   struct tm *utp;
   time_t t;
   time (&t);
   utp = gmtime (&t);

   
   currentSiteNum = 1;
   trackingMode   = LX200_TRACK_DEFAULT;
   lastSet        = -1;
   fault          = false;
   targetRA       = 0;
   targetDEC      = 0;
   currentRA      = 0;
   currentDEC     = 0;
   currentSet     = 0;
   lastMove[0] = lastMove[1] = lastMove[2] = lastMove[3] = 0;

   localTM = new tm;
   
   utp->tm_mon  += 1;
   utp->tm_year += 1900;
   JD = UTtoJD(utp);
   
   IDLog("Julian Day is %g\n", JD);
   
   // Children call parent routines, this is the default
   IDLog("initilizaing from generic LX200 device...\n");
}

void LX200Generic::setCurrectDeviceName(const char * devName)
{
  strcpy(thisDevice, devName);

}

void LX200Generic::ISGetProperties(const char *dev)
{

 if (dev && strcmp (thisDevice, dev))
    return;

  // COMM_GROUP
  IDDefText   (&Port, NULL);
  IDDefSwitch (&PowerSP, NULL);
  IDDefSwitch (&AlignmentSw, NULL);

  // BASIC_GROUP
  IDDefNumber (&elevationLimit, NULL);
  IDDefNumber (&eqNum, NULL);
  IDDefSwitch (&OnCoordSetSw, NULL);
  IDDefSwitch (&abortSlewSw, NULL);
  IDDefSwitch (&ParkSP, NULL);

  // MOVE_GROUP
  IDDefNumber (&TrackingFreq, NULL);
  IDDefSwitch (&SlewModeSw, NULL);
  IDDefSwitch (&TrackModeSw, NULL);
  IDDefSwitch (&MovementSw, NULL);

  // FOCUS_GROUP
  IDDefSwitch(&FocusSpeedSw, NULL);
  IDDefSwitch(&FocusMotionSw, NULL);

  // DATETIME_GROUP
  IDDefText   (&Time, NULL);
  IDDefNumber (&SDTime, NULL);

  // SITE_GROUP
  IDDefSwitch (&SitesSw, NULL);
  IDDefText   (&SiteName, NULL);
  IDDefNumber (&geoNum, NULL);

}

void LX200Generic::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
	double UTCOffset;
	int err;
	struct tm *ltp = new tm;
	struct tm utm;
	time_t ltime;
	time (&ltime);
	localtime_r (&ltime, ltp);
	IText *tp;

	// ignore if not ours //
	if (strcmp (dev, thisDevice))
	    return;

	// suppress warning
	n=n;

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

	if (!strcmp (name, SiteName.name) )
	{
	  if (checkPower(&SiteName))
	   return;

	  if ( ( err = setSiteName(texts[0], currentSiteNum) < 0) )
	  {
	     handleError(&SiteName, err, "Setting site name");
	     return;
	  }
	     SiteName.s = IPS_OK;
	     tp = IUFindText(&SiteName, names[0]);
	     tp->text = new char[strlen(texts[0])+1];
	     strcpy(tp->text, texts[0]);
   	     IDSetText(&SiteName , "Site name updated");
	     return;
       }

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
		
		getSDTime(&STime[0].value);
		IDSetNumber(&SDTime, NULL);
		
		if ( ( err = setUTCOffset(UTCOffset) < 0) )
	  	{
	        Time.s = IPS_IDLE;
	        IDSetText( &Time , "Setting UTC Offset failed.");
		return;
	  	}
		
		if ( ( err = setLocalTime(ltp->tm_hour, ltp->tm_min, ltp->tm_sec) < 0) )
	  	{
	          handleError(&Time, err, "Setting local time");
        	  return;
	  	}

		tp = IUFindText(&Time, names[0]);
		if (!tp)
		 return;
		tp->text = new char[strlen(texts[0])+1];
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
		
		if ( ( err = setCalenderDate(ltp->tm_mday, ltp->tm_mon, ltp->tm_year) < 0) )
	  	{
		  handleError(&Time, err, "Setting local date.");
		  return;
		}
		
 		IDSetText(&Time , "Date changed, updating planetary data...");
	}
}


void LX200Generic::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
	int h =0, m =0, s=0, err;
	double newRA =0, newDEC =0;
	
	// ignore if not ours //
	if (strcmp (dev, thisDevice))
	    return;

	if (!strcmp (name, eqNum.name))
	{
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
	   /*eqNum.s = IPS_BUSY;*/

	   IDLog("We recevined J2000 RA %f - DEC %f\n", newRA, newDEC);;
	   apparentCoord( (double) J2000, JD, &newRA, &newDEC);
	   IDLog("Processed to RA %f - DEC %f\n", newRA, newDEC);

	   if ( (err = setObjectRA(newRA)) < 0 || ( err = setObjectDEC(newDEC)) < 0)
	   {
	     handleError(&eqNum, err, "Setting RA/DEC");
	     return;
	   }

           /*eqNum.s = IPS_BUSY;*/
	   targetRA  = newRA;
	   targetDEC = newDEC;
	   
	   if (MovementSw.s == IPS_BUSY)
	   {
	   	for (int i=0; i < 4; i++)
	   	{
	     		lastMove[i] = 0;
	     		MovementS[i].s = ISS_OFF;
	   	}
		
		MovementSw.s = IPS_IDLE;
		IDSetSwitch(&MovementSw, NULL);
	   }
	   
	   if (handleCoordSet())
	   {
	     eqNum.s = IPS_IDLE;
	     IDSetNumber(&eqNum, NULL);
	     
	   }
	} // end nset
	else
	{
		eqNum.s = IPS_IDLE;
		IDSetNumber(&eqNum, "RA or Dec missing or invalid");
	}

	    return;
     } /* end eqNum */

        if ( !strcmp (name, SDTime.name) )
	{
	  if (checkPower(&SDTime))
	   return;


	  if (values[0] < 0.0 || values[0] > 24.0)
	  {
	    SDTime.s = IPS_IDLE;
	    IDSetNumber(&SDTime , "Time invalid");
	    return;
	  }

	  getSexComponents(values[0], &h, &m, &s);
	  IDLog("Time is %02d:%02d:%02d\n", h, m, s);
	  
	  if ( ( err = setSDTime(h, m, s) < 0) )
	  {
	    handleError(&SDTime, err, "Setting siderial time"); 
            return;
	  }
	  
	  SDTime.np[0].value = values[0];
	  SDTime.s = IPS_OK;

	  IDSetNumber(&SDTime , "Sidereal time updated to %02d:%02d:%02d", h, m, s);

	  return;
        }

	if (!strcmp (name, geoNum.name))
	{
	    // new geographic coords
	    double newLong = 0, newLat = 0;
	    int i, nset;
	    char msg[128];

	  if (checkPower(&geoNum))
	   return;


	    for (nset = i = 0; i < n; i++)
	    {
		INumber *geop = IUFindNumber (&geoNum, names[i]);
		if (geop == &geo[0])
		{
		    newLat = values[i];
		    nset += newLat >= -90.0 && newLat <= 90.0;
		} else if (geop == &geo[1])
		{
		    newLong = values[i];
		    nset += newLong >= 0.0 && newLong < 360.0;
		}
	    }

	    if (nset == 2)
	    {
		char l[32], L[32];
		geoNum.s = IPS_OK;
		fs_sexa (l, newLat, 3, 3600);
		fs_sexa (L, newLong, 4, 3600);
		
		if ( ( err = setSiteLongitude(360.0 - newLong) < 0) )
	        {
		   handleError(&geoNum, err, "Setting site coordinates");
		   return;
	         }
		
		setSiteLatitude(newLat);
		geoNum.np[0].value = newLat;
		geoNum.np[1].value = newLong;
		sprintf (msg, "Site location updated to Lat %s - Long %s", l, L);
	    } else
	    {
		geoNum.s = IPS_IDLE;
		sprintf (msg, "Lat or Long missing or invalid");
	    }
	    IDSetNumber (&geoNum, msg);
	    return;
	}

	if ( !strcmp (name, TrackingFreq.name) )
	{

	 if (checkPower(&TrackingFreq))
	  return;

	  IDLog("Trying to set track freq of: %f\n", values[0]);

	  if ( ( err = setTrackFreq(values[0])) < 0) 
	  {
             handleError(&TrackingFreq, err, "Setting tracking frequency");
	     return;
	 }
	 
	 TrackingFreq.s = IPS_OK;
	 TrackingFreq.np[0].value = values[0];
	 IDSetNumber(&TrackingFreq, "Tracking frequency set to %04.1f", values[0]);
	 if (trackingMode != LX200_TRACK_MANUAL)
	 {
	      trackingMode = LX200_TRACK_MANUAL;
	      TrackModeS[0].s = ISS_OFF;
	      TrackModeS[1].s = ISS_OFF;
	      TrackModeS[2].s = ISS_ON;
	      TrackModeSw.s   = IPS_OK;
	      selectTrackingMode(trackingMode);
	      IDSetSwitch(&TrackModeSw, NULL);
	 }
	 
	  return;
	}

	if (!strcmp (name, elevationLimit.name))
	{
	    // new elevation limits
	    double minAlt = 0, maxAlt = 0;
	    int i, nset;

	  if (checkPower(&elevationLimit))
	   return;

	    for (nset = i = 0; i < n; i++)
	    {
		INumber *altp = IUFindNumber (&elevationLimit, names[i]);
		if (altp == &altLimit[0])
		{
		    minAlt = values[i];
		    nset += minAlt >= -90.0 && minAlt <= 90.0;
		} else if (altp == &altLimit[1])
		{
		    maxAlt = values[i];
		    nset += maxAlt >= -90.0 && maxAlt <= 90.0;
		}
	    }
	    if (nset == 2)
	    {
		//char l[32], L[32];
		if ( ( err = setMinElevationLimit( (int) minAlt) < 0) )
	 	{
	         handleError(&elevationLimit, err, "Setting elevation limit");
	 	}
		setMaxElevationLimit( (int) maxAlt);
		elevationLimit.np[0].value = minAlt;
		elevationLimit.np[1].value = maxAlt;
		elevationLimit.s = IPS_OK;
		IDSetNumber (&elevationLimit, NULL);
	    } else
	    {
		elevationLimit.s = IPS_IDLE;
		IDSetNumber(&elevationLimit, "elevation limit missing or invalid");
	    }

	    return;
	}

}

void LX200Generic::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
	int index;
	int dd, mm, err;
	char combinedDir[64];
	ISwitch *swp;

	// suppress warning
	names = names;

	//IDLog("in new Switch with Device= %s and Property= %s and #%d items\n", dev, name,n);
	//IDLog("SolarSw name is %s\n", SolarSw.name);

	IDLog("The device name is %s\n", dev);
	// ignore if not ours //
	if (strcmp (thisDevice, dev))
	    return;

	// FIRST Switch ALWAYS for power
	if (!strcmp (name, PowerSP.name))
	{
	 IUResetSwitches(&PowerSP);
	 IUUpdateSwitches(&PowerSP, states, names, n);
   	 powerTelescope();
	 return;
	}

	// Coord set
	if (!strcmp(name, OnCoordSetSw.name))
	{
  	  if (checkPower(&OnCoordSetSw))
	   return;

	  IUResetSwitches(&OnCoordSetSw);
	  IUUpdateSwitches(&OnCoordSetSw, states, names, n);
	  currentSet = getOnSwitch(&OnCoordSetSw);
	  OnCoordSetSw.s = IPS_OK;
	  IDSetSwitch(&OnCoordSetSw, NULL);
	}
	
	// Parking
	if (!strcmp(name, ParkSP.name))
	{
	  if (checkPower(&ParkSP))
	    return;
           
	   ParkSP.s = IPS_IDLE;
	   

	   if ( (err = getSDTime(&STime[0].value)) < 0)
	   {
  	  	handleError(&ParkSP, err, "Get siderial time");
	  	return;
	   }
	   
	   if (AlignmentS[0].s == ISS_ON)
	   {
	     targetRA  = STime[0].value;
	     targetDEC = 0;
	     setObjectRA(targetRA);
	     setObjectDEC(targetDEC);
	   }
	   
	   else if (AlignmentS[1].s == ISS_ON)
	   {
	     targetRA  = calculateRA(STime[0].value);
	     targetDEC = calculateDec(geo[0].value, STime[0].value);
	     setObjectRA(targetRA);
	     setObjectDEC(targetDEC);
	     IDLog("Parking the scope in AltAz (0,0) which corresponds to (RA,DEC) of (%g,%g)\n", targetRA, targetDEC);
	     IDLog("Current Sidereal time is: %g\n", STime[0].value);
	     IDSetNumber(&SDTime, NULL);
	   }
	   else
	   {
	     IDSetSwitch(&ParkSP, "You can only park the telescope in Polar or AltAz modes.");
	     return;
	   }
	   
	   IDSetNumber(&SDTime, NULL);
	   
	   currentSet = LX200_PARK;
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
	  
	  IUResetSwitches(&abortSlewSw);
	  abortSlew();

	    if (eqNum.s == IPS_BUSY)
	    {
		abortSlewSw.s = IPS_OK;
		eqNum.s       = IPS_IDLE;
		IDSetSwitch(&abortSlewSw, "Slew aborted.");
		IDSetNumber(&eqNum, NULL);
            }
	    else if (MovementSw.s == IPS_BUSY)
	    {
	        
		for (int i=0; i < 4; i++)
		  lastMove[i] = 0;
		
		MovementSw.s  = IPS_IDLE; 
		abortSlewSw.s = IPS_OK;		
		eqNum.s       = IPS_IDLE;
		IUResetSwitches(&MovementSw);
		IUResetSwitches(&abortSlewSw);
		IDSetSwitch(&abortSlewSw, "Slew aborted.");
		IDSetSwitch(&MovementSw, NULL);
		IDSetNumber(&eqNum, NULL);
	    }
	    else
	    {
	        IUResetSwitches(&MovementSw);
	        abortSlewSw.s = IPS_IDLE;
	        IDSetSwitch(&abortSlewSw, NULL);
	    }

	    return;
	}

	// Alignment
	if (!strcmp (name, AlignmentSw.name))
	{
	  if (checkPower(&AlignmentSw))
	   return;

	  IUResetSwitches(&AlignmentSw);
	  IUUpdateSwitches(&AlignmentSw, states, names, n);
	  index = getOnSwitch(&AlignmentSw);

	  if ( ( err = setAlignmentMode(index) < 0) )
	  {
	     handleError(&AlignmentSw, err, "Setting alignment");
             return;
	  }
	  
	  AlignmentSw.s = IPS_OK;
          IDSetSwitch (&AlignmentSw, NULL);
	  return;

	}

        // Sites
	if (!strcmp (name, SitesSw.name))
	{
	  if (checkPower(&SitesSw))
	   return;

	  IUResetSwitches(&SitesSw);
	  IUUpdateSwitches(&SitesSw, states, names, n);
	  currentSiteNum = getOnSwitch(&SitesSw) + 1;
	  
	  if ( ( err = selectSite(currentSiteNum) < 0) )
	  {
   	      handleError(&SitesSw, err, "Selecting sites");
	      return;
	  }

	  if ( ( err = getSiteLatitude(&dd, &mm) < 0))
	  {
	      handleError(&SitesSw, err, "Selecting sites");
	      return;
	  }

	  geoNum.np[0].value = dd + mm / 60.0;
	  
	  if ( ( err = getSiteLongitude(&dd, &mm) < 0))
	  {
	        handleError(&SitesSw, err, "Selecting sites");
		return;
	  }
	  
	  geoNum.np[1].value = dd + mm / 60.0;
	  
	  getSiteName( SiteName.tp[0].text, currentSiteNum);

	  IDLog("Selecting site %d\n", currentSiteNum);
	  
	  geoNum.s = SiteName.s = SitesSw.s = IPS_OK;

	  IDSetNumber (&geoNum, NULL);
	  IDSetText   (&SiteName, NULL);
          IDSetSwitch (&SitesSw, NULL);
	  return;
	}

	// Focus speed
	if (!strcmp (name, FocusSpeedSw.name))
	{
	  if (checkPower(&FocusSpeedSw))
	   return;
	   
	   IUResetSwitches(&FocusSpeedSw);
	   IUUpdateSwitches(&FocusSpeedSw, states, names, n);
	   index = getOnSwitch(&FocusSpeedSw);
	  
	  if ( ( err = setFocuserSpeedMode(index) < 0) )
	  {
	        handleError(&FocusSpeedSw, err, "Setting focuser speed mode"); 
		return;
	  }
	  
	  FocusSpeedSw.s = IPS_OK;
	  IDSetSwitch(&FocusSpeedSw, NULL);
	  return;
	}

	// Focus Motion
	if (!strcmp (name, FocusMotionSw.name))
	{
	  if (checkPower(&FocusMotionSw))
	   return;

	  IUResetSwitches(&FocusMotionSw);
	  IUUpdateSwitches(&FocusMotionSw, states, names, n);
	  index = getOnSwitch(&FocusMotionSw);
	  
	  if ( ( err = setFocuserMotion(index) < 0) )
	  {
	     handleError(&FocusMotionSw, err, "Setting focuser speed");
             return;
	  }
	  
	  FocusMotionSw.s = IPS_OK;
	  IDSetSwitch(&FocusMotionSw, NULL);
	  return;
	}

	// Slew mode
	if (!strcmp (name, SlewModeSw.name))
	{
	  if (checkPower(&SlewModeSw))
	   return;

	  IUResetSwitches(&SlewModeSw);
	  IUUpdateSwitches(&SlewModeSw, states, names, n);
	  index = getOnSwitch(&SlewModeSw);
	   
	  if ( ( err = setSlewMode(index) < 0) )
	  {
              handleError(&SlewModeSw, err, "Setting slew mode");
              return;
	  }
	  
          SlewModeSw.s = IPS_OK;
	  IDSetSwitch(&SlewModeSw, NULL);
	  return;
	}

	// Movement
	if (!strcmp (name, MovementSw.name))
	{
	  if (checkPower(&MovementSw))
	   return;

	 index = -1;
	 IUUpdateSwitches(&MovementSw, states, names, n);
	 swp = IUFindSwitch(&MovementSw, names[0]);
	 
	 if (!swp)
	 {
	    abortSlew();
	    IUResetSwitches(&MovementSw);
	    MovementSw.s = IPS_IDLE;
	    IDSetSwitch(&MovementSw, NULL);
	 }
	 
	 if (swp == &MovementS[0]) index = 0;
	 else if (swp == &MovementS[1]) index = 1;
	 else if (swp == &MovementS[2]) index = 2;
	 else index = 3;
	 
	 lastMove[index] = lastMove[index] == 0 ? 1 : 0;
	 if (lastMove[index] == 0)
	   MovementS[index].s = ISS_OFF;
	     
	   // North/South movement is illegal
	   if (lastMove[LX200_NORTH] && lastMove[LX200_SOUTH])	
	   {
	     abortSlew();
	      for (int i=0; i < 4; i++)
	        lastMove[i] = 0;
	      	
	      IUResetSwitches(&MovementSw);
	      MovementSw.s       = IPS_IDLE;
	      IDSetSwitch(&MovementSw, "Slew aborted.");
	      return;
	   }
	   
	   // East/West movement is illegal
	   if (lastMove[LX200_EAST] && lastMove[LX200_WEST])	
	   {
	      abortSlew();
	      for (int i=0; i < 4; i++)
	            lastMove[i] = 0;
	       
	      IUResetSwitches(&MovementSw);
     	      MovementSw.s       = IPS_IDLE;
	      IDSetSwitch(&MovementSw, "Slew aborted.");
	      return;
	   }
	      
          IDLog("We have switch %d \n ", index);
	  IDLog("NORTH: %d -- WEST: %d -- EAST: %d -- SOUTH %d\n", lastMove[0], lastMove[1], lastMove[2], lastMove[3]);

	  if (lastMove[index] == 1)
	  {
	        IDLog("issuing a move command\n");
	    	if ( ( err = MoveTo(index) < 0) )
	  	{
	        	 handleError(&MovementSw, err, "Setting motion direction");
 		 	return;
	  	}
	  }
	  else
	     HaltMovement(index);

          if (!lastMove[0] && !lastMove[1] && !lastMove[2] && !lastMove[3])
	     MovementSw.s = IPS_IDLE;
	  
	  if (lastMove[index] == 0)
	     IDSetSwitch(&MovementSw, "Moving toward %s aborted.", Direction[index]);
	  else
	  {
	     MovementSw.s = IPS_BUSY;
	     if (lastMove[LX200_NORTH] && lastMove[LX200_WEST])
	       strcpy(combinedDir, "North West");
	     else if (lastMove[LX200_NORTH] && lastMove[LX200_EAST])
	       strcpy(combinedDir, "North East");
	     else if (lastMove[LX200_SOUTH] && lastMove[LX200_WEST])
	       strcpy(combinedDir, "South West");
	     else if (lastMove[LX200_SOUTH] && lastMove[LX200_EAST])
	       strcpy(combinedDir, "South East");
	     else 
	       strcpy(combinedDir, Direction[index]);
	     
	     IDSetSwitch(&MovementSw, "Moving %s...", combinedDir);
	  }
	  return;
	}

	// Tracking mode
	if (!strcmp (name, TrackModeSw.name))
	{
	  if (checkPower(&TrackModeSw))
	   return;

	  IUResetSwitches(&TrackModeSw);
	  IUUpdateSwitches(&TrackModeSw, states, names, n);
	  trackingMode = getOnSwitch(&TrackModeSw);
	  
	  if ( ( err = selectTrackingMode(trackingMode) < 0) )
	  {
	         handleError(&TrackModeSw, err, "Setting tracking mode.");
		 return;
	  }
	  
          getTrackFreq(&TrackFreq[0].value);
	  TrackModeSw.s = IPS_OK;
	  IDSetNumber(&TrackingFreq, NULL);
	  IDSetSwitch(&TrackModeSw, NULL);
	  return;
	}

}

void LX200Generic::handleError(ISwitchVectorProperty *svp, int err, const char *msg)
{
  char errmsg[1024];
  
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

void LX200Generic::handleError(INumberVectorProperty *nvp, int err, const char *msg)
{
  char errmsg[1024];
  
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

void LX200Generic::handleError(ITextVectorProperty *tvp, int err, const char *msg)
{
  char errmsg[1024];
  
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

 void LX200Generic::correctFault()
 {
 
   fault = false;
   IDMessage(thisDevice, "Telescope is online.");
   
 }

bool LX200Generic::isTelescopeOn(void)
{
  return (PowerSP.sp[0].s == ISS_ON);
}

static void retryConnection(void * p)
{
  p=p;
  
  if (testTelescope())
  {
    PowerSP.s = IPS_IDLE;
    IDSetSwitch(&PowerSP, "The connection to the telescope is lost.");
    return;
  }
  
  PowerS[0].s = ISS_ON;
  PowerS[1].s = ISS_OFF;
  PowerSP.s = IPS_OK;
   
  IDSetSwitch(&PowerSP, "The connection to the telescope has been resumed.");

}

void LX200Generic::ISPoll()
{
        double dx, dy;
	int err=0;
	
	if (!isTelescopeOn())
	 return;

	switch (eqNum.s)
	{
	case IPS_IDLE:
	getLX200RA(&currentRA);
	getLX200DEC(&currentDEC);
	
        if ( fabs (currentRA - lastRA) > 0.01 || fabs (currentDEC - lastDEC) > 0.01)
	{
	        eqNum.np[0].value = lastRA = currentRA;
		eqNum.np[1].value = lastDEC = currentDEC;
		IDSetNumber (&eqNum, NULL);
	}
        break;

        case IPS_BUSY:
	    getLX200RA(&currentRA);
	    getLX200DEC(&currentDEC);
	    dx = targetRA - currentRA;
	    dy = targetDEC - currentDEC;

	    IDLog("targetRA is %g, currentRA is %g\n", targetRA, currentRA);
	    IDLog("targetDEC is %g, currentDEC is %g\n*************************\n", targetDEC, currentDEC);

	    eqNum.np[0].value = currentRA;
	    eqNum.np[1].value = currentDEC;

	    // Wait until acknowledged or within threshold
	    if (fabs(dx) <= RA_THRESHOLD && fabs(dy) <= DEC_THRESHOLD)
	    {
		currentRA = targetRA;
		currentDEC = targetDEC;
		apparentCoord( JD, (double) J2000, &currentRA, &currentDEC);
		eqNum.np[0].value = currentRA;
		eqNum.np[1].value = currentDEC;
		IUResetSwitches(&OnCoordSetSw);
		OnCoordSetSw.s = IPS_OK;
		eqNum.s = IPS_OK;
		IDSetNumber (&eqNum, NULL);

		switch (currentSet)
		{
		  case LX200_SLEW:
		  	OnCoordSetSw.sp[0].s = ISS_ON;
		  	IDSetSwitch (&OnCoordSetSw, "Slew is complete.");
		  	break;
		  
		  case LX200_TRACK:
		  	OnCoordSetSw.sp[1].s = ISS_ON;
		  	IDSetSwitch (&OnCoordSetSw, "Slew is complete. Tracking...");
			break;
		  
		  case LX200_SYNC:
		  	break;
		  
		  case LX200_PARK:
		        if (setSlewMode(LX200_SLEW_GUIDE) < 0)
			{ 
			  handleError(&eqNum, err, "Setting slew mode");
			  return;
			}
			
			IUResetSwitches(&SlewModeSw);
			SlewModeS[LX200_SLEW_GUIDE].s = ISS_ON;
			IDSetSwitch(&SlewModeSw, NULL);
			
			MoveTo(LX200_EAST);
			IUResetSwitches(&MovementSw);
			MovementS[LX200_EAST].s = ISS_ON;
			MovementSw.s = IPS_BUSY;
			for (int i=0; i < 4; i++)
			  lastMove[i] = 0;
			lastMove[LX200_EAST] = 1;
			IDSetSwitch(&MovementSw, NULL);
			
			ParkSP.s = IPS_OK;
		  	IDSetSwitch (&ParkSP, "Park is complete. Turn off the telescope now.");
		  	break;
		}

	    } else
		IDSetNumber (&eqNum, NULL);
	    break;

	case IPS_OK:
	if ( (err = getLX200RA(&currentRA)) < 0 || (err = getLX200DEC(&currentDEC)) < 0)
	{
	  handleError(&eqNum, err, "Getting RA/DEC");
	  return;
	}
	
	if (fault)
	  correctFault();

        if ( fabs (currentRA - lastRA) > 0.01 || fabs (currentDEC - lastDEC) > 0.01)
	{
		eqNum.np[0].value = currentRA;
		eqNum.np[1].value = currentDEC;
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
	   getLX200RA(&currentRA);
	   getLX200DEC(&currentDEC);
	   apparentCoord( JD, (double) J2000, &currentRA, &currentDEC);
	   eqNum.np[0].value = currentRA;
	   eqNum.np[1].value = currentDEC;

	   IDSetNumber (&eqNum, NULL);
	     break;
	 case IPS_OK:
	   break;
	 case IPS_ALERT:
	   break;
	 }

}

void LX200Generic::getBasicData()
{

  int dd, mm;
  int err;
  struct tm *timep;
  time_t ut;
  time (&ut);
  timep = gmtime (&ut);
  strftime (Time.tp[0].text, sizeof(Time.tp[0].text), "%Y-%m-%dT%H:%M:%S", timep);

  IDLog("time is %s\n", Time.tp[0].text);

  getAlignment();
  
  checkLX200Format();
  
  if ( (err = getTimeFormat(&timeFormat)) < 0)
     IDMessage(thisDevice, "Failed to retrieve time format from device.");
  else
  {
    timeFormat = (timeFormat == 24) ? LX200_24 : LX200_AM;
    // We always do 24 hours
    if (timeFormat != LX200_24)
      toggleTimeFormat();
  }

  getLX200RA(&targetRA);
  getLX200DEC(&targetDEC);

  eqNum.np[0].value = targetRA;
  eqNum.np[1].value = targetDEC;
  
  IDSetNumber (&eqNum, NULL);

  if ( (err = getSDTime(&STime[0].value)) < 0)
    IDMessage(thisDevice, "Failed to retrieve siderial time from device.");
  else
    IDSetNumber (&SDTime, NULL);
    
     
  if ( (err = selectSite(currentSiteNum)) < 0)
    IDMessage(thisDevice, "Failed to select a site from device.");
  else
   IDSetSwitch (&SitesSw, NULL);

  
  if ( (err = getSiteLatitude(&dd, &mm)) < 0)
    IDMessage(thisDevice, "Failed to get site latitude from device.");
  else
    geoNum.np[0].value = dd + mm/60.0;
  
  if ( (err = getSiteLongitude(&dd, &mm)) < 0)
    IDMessage(thisDevice, "Failed to get site longitude from device.");
  else
    geoNum.np[1].value = dd + mm/60.0;

  IDSetNumber (&geoNum, NULL);
  
  SiteNameT[0].text = new char[64];
  
  if ( (err = getSiteName(SiteNameT[0].text, currentSiteNum)) < 0)
    IDMessage(thisDevice, "Failed to get site name from device");
  else
    IDSetText   (&SiteName, NULL);
  
  if ( (err = getTrackFreq(&TrackFreq[0].value)) < 0)
     IDMessage(thisDevice, "Failed to get tracking frequency from device.");
  else
     IDSetNumber (&TrackingFreq, NULL);
  
  IDSetText   (&Time, NULL);
  
  
}

int LX200Generic::handleCoordSet()
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
	  if (eqNum.s == IPS_BUSY)
	  {
	     abortSlew();

	     // sleep for 100 mseconds
	     usleep(100000);
	  }

	  if ((err = Slew()))
	  {
	    slewError(err);
	    return (-1);
	  }

	  eqNum.s = IPS_BUSY;
	  fs_sexa(RAStr, targetRA, 2, 3600);
	  fs_sexa(DecStr, targetDEC, 2, 3600);
	  IDSetNumber(&eqNum, "Slewing to J2000 RA %s - DEC %s", RAStr, DecStr);
	  break;

     // Track
     case LX200_TRACK:
          if (eqNum.s == IPS_BUSY)
	  {
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
		eqNum.s = IPS_BUSY;
		IDSetNumber(&eqNum, "Slewing to J2000 RA %s - DEC %s", RAStr, DecStr);
	  }
	  else
	  {
	    IDLog("Tracking called, but tracking threshold not reached yet.\n");
	    eqNum.s = IPS_OK;
	    eqNum.np[0].value = currentRA;
	    eqNum.np[1].value = currentDEC;

	    if (lastSet != LX200_TRACK)
	      IDSetNumber(&eqNum, "Tracking...");
	    else
	      IDSetNumber(&eqNum, NULL);
	  }
	  lastSet = LX200_TRACK;
      break;

    // Sync
    case LX200_SYNC:
          lastSet = LX200_SYNC;
	  eqNum.s = IPS_IDLE;
	   
	  if ( ( err = Sync(syncString) < 0) )
	  {
	        IDSetNumber( &eqNum , "Synchronization failed.");
		return (-1);
	  }

	  eqNum.s = IPS_OK;
	  IDLog("Synchronization successful %s\n", syncString);
	  IDSetNumber(&eqNum, "Synchronization successful.");
	  break;

   // PARK
   // Set RA to LST and DEC to 0 degrees, slew, then change to 'guide' slew after slew is complete.
   case LX200_PARK:
          if (eqNum.s == IPS_BUSY)
	  {
	     abortSlew();

	     // sleep for 200 mseconds
	     usleep(200000);
	  }

	  if ((err = Slew()))
	  {
	    	slewError(err);
	    	return (-1);
	  }
		
	  ParkSP.s = IPS_BUSY;
	  eqNum.s = IPS_BUSY;
	  IDSetNumber(&eqNum, NULL);
	  IDSetSwitch(&ParkSP, "The telescope is slewing to park position. Turn off the telescope after park is complete.");
	  
	  break;
	  
   }
   
   return (0);

}

int LX200Generic::getOnSwitch(ISwitchVectorProperty *sp)
{
 for (int i=0; i < sp->nsp ; i++)
     if (sp->sp[i].s == ISS_ON)
      return i;

 return -1;
}


int LX200Generic::checkPower(ISwitchVectorProperty *sp)
{
  if (PowerSP.s != IPS_OK)
  {
    IDMessage (thisDevice, "Cannot change a property while the telescope is offline.");
    sp->s = IPS_IDLE;
    IDSetSwitch(sp, NULL);
    return -1;
  }

  return 0;
}

int LX200Generic::checkPower(INumberVectorProperty *np)
{

  if (PowerSP.s != IPS_OK)
  {
    IDMessage (thisDevice, "Cannot change a property while the telescope is offline.");
    np->s = IPS_IDLE;
    IDSetNumber(np, NULL);
    return -1;
  }

  return 0;

}

int LX200Generic::checkPower(ITextVectorProperty *tp)
{

  if (PowerSP.s != IPS_OK)
  {
    IDMessage (thisDevice, "Cannot change a property while the telescope is offline.");
    tp->s = IPS_IDLE;
    IDSetText(tp, NULL);
    return -1;
  }

  return 0;

}

void LX200Generic::powerTelescope()
{
     switch (PowerSP.sp[0].s)
     {
      case ISS_ON:

         if (Connect(Port.tp[0].text))
	 {
	   PowerS[0].s = ISS_OFF;
	   PowerS[1].s = ISS_ON;
	   IDSetSwitch (&PowerSP, "Error connecting to port %s\n", Port.tp[0].text);
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
         IDSetSwitch (&PowerSP, "Telescope is offline.");
	 IDLog("Telescope is offline.");
	 Disconnect();
	 break;

    }

}

void LX200Generic::slewError(int slewCode)
{
    OnCoordSetSw.s = IPS_IDLE;
    ParkSP.s = IPS_IDLE;
    
    IDSetSwitch(&ParkSP, NULL);

    if (slewCode == 1)
	IDSetSwitch (&OnCoordSetSw, "Object below horizon.");
    else if (slewCode == 2)
	IDSetSwitch (&OnCoordSetSw, "Object below the minimum elevation limit.");
    else
        IDSetSwitch (&OnCoordSetSw, "Slew failed.");
    

}

void LX200Generic::getAlignment()
{

   if (PowerSP.s != IPS_OK)
    return;

   char align = ACK();
   if (align < 0)
   {
     IDSetSwitch (&AlignmentSw, "Failed to get telescope alignment.");
     return;
   }

   AlignmentS[0].s = ISS_OFF;
   AlignmentS[1].s = ISS_OFF;
   AlignmentS[2].s = ISS_OFF;

    switch (align)
    {
      case 'P': AlignmentS[0].s = ISS_ON;
       		break;
      case 'A': AlignmentS[1].s = ISS_ON;
      		break;
      case 'L': AlignmentS[2].s = ISS_ON;
            	break;
    }

    AlignmentSw.s = IPS_OK;
    IDSetSwitch (&AlignmentSw, NULL);
    IDLog("ACK success %c\n", align);
}
