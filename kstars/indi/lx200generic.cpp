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
#include "lx200gps.h"
#include "lx200classic.h"

/*
** Return the timezone offset in hours (as a double, so fractional
** hours are possible, for instance in Newfoundland). Also sets
** daylight on non-Linux systems to record whether DST is in effect.
*/


#if !(TIMEZONE_IS_INT)
static int daylight = 0;
#endif

static inline double timezoneOffset()
{
/* 
** In Linux, there's a timezone variable that holds the timezone offset;
** Otherwise, we need to make a little detour. The directions of the offset
** are different: CET is -3600 in Linux and +3600 elsewhere.
*/
#if TIMEZONE_IS_INT
  return timezone / (60 * 60);
#else
  time_t now;
  struct tm *tm;
  now = time(NULL);
  tm = localtime(&now);
  daylight = tm->tm_isdst;
  return -(tm->tm_gmtoff) / (60 * 60);
#endif
}

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
#define BASIC_GROUP	"Main Control"
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
static ISwitch PowerS[]          = {{"CONNECT" , "Connect" , ISS_OFF, 0, 0},{"DISCONNECT", "Disconnect", ISS_ON, 0, 0}};
static ISwitch AlignmentS []     = {{"Polar", "", ISS_ON, 0, 0}, {"AltAz", "", ISS_OFF, 0, 0}, {"Land", "", ISS_OFF, 0, 0}};
static ISwitch SitesS[]          = {{"Site 1", "", ISS_ON, 0, 0}, {"Site 2", "", ISS_OFF, 0, 0},  {"Site 3", "", ISS_OFF, 0, 0},  {"Site 4", "", ISS_OFF, 0 ,0}};
static ISwitch SlewModeS[]       = {{"Max", "", ISS_ON, 0, 0}, {"Find", "", ISS_OFF, 0, 0}, {"Centering", "", ISS_OFF, 0, 0}, {"Guide", "", ISS_OFF, 0 , 0}};
static ISwitch OnCoordSetS[]     = {{"SLEW", "Slew", ISS_ON, 0, 0 }, {"TRACK", "Track", ISS_OFF, 0, 0}, {"SYNC", "Sync", ISS_OFF, 0 , 0}};
static ISwitch TrackModeS[]      = {{ "Default", "", ISS_ON, 0, 0} , { "Lunar", "", ISS_OFF, 0, 0}, {"Manual", "", ISS_OFF, 0, 0}};
static ISwitch abortSlewS[]      = {{"ABORT", "Abort", ISS_OFF, 0, 0 }};
static ISwitch ParkS[]		 = { {"PARK", "Park", ISS_OFF, 0, 0} };

static ISwitch MovementS[]       = {{"N", "North", ISS_OFF, 0, 0}, {"W", "West", ISS_OFF, 0, 0}, {"E", "East", ISS_OFF, 0, 0}, {"S", "South", ISS_OFF, 0, 0}};

static INumber	FocusSpeedN[]	 = {{"SPEED", "Speed", "%0.f", 0., 3., 1., 0.}};
static ISwitch  FocusMotionS[]	 = { {"IN", "Focus in", ISS_OFF, 0, 0}, {"OUT", "Focus out", ISS_OFF, 0, 0}};
static INumber  FocusTimerN[]    = { {"TIMER", "Timer (s)", "%10.6m", 0., 120., 1., 0., 0, 0, 0 }};

static INumberVectorProperty FocusTimerNP = { mydev, "FOCUS_TIMER", "Focus Timer", FOCUS_GROUP, IP_RW, 0, IPS_IDLE, FocusTimerN, NARRAY(FocusTimerN), "", 0};

/* equatorial position */
INumber eq[] = {
    {"RA",  "RA  H:M:S", "%10.6m",  0., 24., 0., 0., 0, 0, 0},
    {"DEC", "Dec D:M:S", "%10.6m", -90., 90., 0., 0., 0, 0, 0},
};
//TODO decide appropiate TIME_OUT
// N.B. No Static identifier as it is needed for external linkage
INumberVectorProperty eqNum = {
    mydev, "EQUATORIAL_EOD_COORD", "Equatorial JNow", BASIC_GROUP, IP_RW, 0, IPS_IDLE,
    eq, NARRAY(eq), "", 0};

/* Fundamental group */
ISwitchVectorProperty PowerSP		= { mydev, "CONNECTION" , "Connection", COMM_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, PowerS, NARRAY(PowerS), "", 0};
static IText PortT[]			= {{"PORT", "Port", 0, 0, 0, 0}};
static ITextVectorProperty Port		= { mydev, "DEVICE_PORT", "Ports", COMM_GROUP, IP_RW, 0, IPS_IDLE, PortT, NARRAY(PortT), "", 0};

/* Basic data group */
static ISwitchVectorProperty AlignmentSw	= { mydev, "Alignment", "", COMM_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, AlignmentS, NARRAY(AlignmentS), "", 0};

/* Movement group */
static ISwitchVectorProperty OnCoordSetSw    = { mydev, "ON_COORD_SET", "On Set", BASIC_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, OnCoordSetS, NARRAY(OnCoordSetS), "", 0};

static ISwitchVectorProperty abortSlewSw     = { mydev, "ABORT_MOTION", "Abort Slew/Track", BASIC_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, abortSlewS, NARRAY(abortSlewS), "", 0};

ISwitchVectorProperty	ParkSP = {mydev, "PARK", "Park Scope", BASIC_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, ParkS, NARRAY(ParkS), "", 0 };

static ISwitchVectorProperty SlewModeSw      = { mydev, "Slew rate", "", MOVE_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, SlewModeS, NARRAY(SlewModeS), "", 0};

static ISwitchVectorProperty TrackModeSw  = { mydev, "Tracking Mode", "", MOVE_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, TrackModeS, NARRAY(TrackModeS), "", 0};

static INumber TrackFreq[]  = {{ "trackFreq", "Freq", "%g", 56.4, 60.1, 0.1, 60.1, 0, 0, 0}};

static INumberVectorProperty TrackingFreq= { mydev, "Tracking Frequency", "", MOVE_GROUP, IP_RW, 0, IPS_IDLE, TrackFreq, NARRAY(TrackFreq), "", 0};

static ISwitchVectorProperty MovementSw      = { mydev, "MOVEMENT", "Move toward", MOVE_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, MovementS, NARRAY(MovementS), "", 0};

// Focus Control
static INumberVectorProperty	FocusSpeedNP  = {mydev, "FOCUS_SPEED", "Speed", FOCUS_GROUP, IP_RW, 0, IPS_IDLE, FocusSpeedN, NARRAY(FocusSpeedN), "", 0};

static ISwitchVectorProperty	FocusMotionSw = {mydev, "FOCUS_MOTION", "Motion", FOCUS_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, FocusMotionS, NARRAY(FocusMotionS), "", 0};

/* Data & Time */
static IText UTC[] = {{"UTC", "UTC", 0, 0, 0, 0}};
ITextVectorProperty Time = { mydev, "TIME", "UTC Time", DATETIME_GROUP, IP_RW, 0, IPS_IDLE, UTC, NARRAY(UTC), "", 0};
static INumber STime[] = {{"LST", "Sidereal time", "%10.6m" , 0.,24.,0.,0., 0, 0, 0}};
INumberVectorProperty SDTime = { mydev, "SDTIME", "Sidereal Time", DATETIME_GROUP, IP_RW, 0, IPS_IDLE, STime, NARRAY(STime), "", 0};


/* Site managment */
static ISwitchVectorProperty SitesSw  = { mydev, "Sites", "", SITE_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, SitesS, NARRAY(SitesS), "", 0};
/* geographic location */
static INumber geo[] = {
    {"LAT",  "Lat.  D:M:S +N", "%10.6m",  -90.,  90., 0., 0., 0, 0, 0},
    {"LONG", "Long. D:M:S +E", "%10.6m", 0., 360., 0., 0., 0, 0, 0},
};
static INumberVectorProperty geoNum = {
    mydev, "GEOGRAPHIC_COORD", "Geographic Location", SITE_GROUP, IP_RW, 0., IPS_IDLE,
    geo, NARRAY(geo), "", 0};
static IText   SiteNameT[] = {{"SiteName", "", 0, 0, 0, 0}};
static ITextVectorProperty SiteName = { mydev, "Site Name", "", SITE_GROUP, IP_RW, 0 , IPS_IDLE, SiteNameT, NARRAY(SiteNameT), "", 0};

void changeLX200GenericDeviceName(const char * newName)
{
  strcpy(PowerSP.device , newName);
  strcpy(Port.device , newName);
  strcpy(AlignmentSw.device, newName);

  // BASIC_GROUP
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
  strcpy(FocusSpeedNP.device , newName );
  strcpy(FocusMotionSw.device , newName );
  strcpy(FocusTimerNP.device, newName);

  // DATETIME_GROUP
  strcpy(Time.device , newName );
  strcpy(SDTime.device , newName );

  // SITE_GROUP
  strcpy(SitesSw.device , newName );
  strcpy(SiteName.device , newName );
  strcpy(geoNum.device , newName );
  
}

void changeAllDeviceNames(const char *newName)
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
 
  PortT[0].text = strcpy(new char[32], "/dev/ttyS0");
  UTC[0].text   = strcpy(new char[32], "YYYY-MM-DDTHH:MM:SS");
  
  if (strstr(me, "lx200classic"))
  {
     fprintf(stderr , "initilizaing from LX200 classic device...\n");
     // 1. mydev = device_name
     changeAllDeviceNames("LX200 Classic");
     // 2. device = sub_class
     telescope = new LX200Classic();
     telescope->setCurrentDeviceName("LX200 Classic");

     MaxReticleFlashRate = 3;
  }

  else if (strstr(me, "lx200gps"))
  {
     fprintf(stderr , "initilizaing from LX200 GPS device...\n");
     // 1. mydev = device_name
     changeAllDeviceNames("LX200 GPS");
     // 2. device = sub_class
     telescope = new LX200GPS();
     telescope->setCurrentDeviceName("LX200 GPS");

     MaxReticleFlashRate = 9;
  }
  else if (strstr(me, "lx200_16"))
  {

    IDLog("Initilizaing from LX200 16 device...\n");
    // 1. mydev = device_name
    changeAllDeviceNames("LX200 16");
    // 2. device = sub_class
   telescope = new LX200_16();
   telescope->setCurrentDeviceName("LX200 16");

   MaxReticleFlashRate = 3;
 }
 else if (strstr(me, "lx200autostar"))
 {
   fprintf(stderr , "initilizaing from autostar device...\n");
  
   // 1. change device name
   changeAllDeviceNames("LX200 Autostar");
   // 2. device = sub_class
   telescope = new LX200Autostar();
   telescope->setCurrentDeviceName("LX200 Autostar");

   MaxReticleFlashRate = 9;
 }
 // be nice and give them a generic device
 else
 {
  telescope = new LX200Generic();
  telescope->setCurrentDeviceName("LX200 Generic");
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
void ISNewBLOB (const char */*dev*/, const char */*name*/, int */*sizes[]*/, char **/*blobs[]*/, char **/*formats[]*/, char **/*names[]*/, int /*n*/)
{}

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
   simulation     = false;
   targetRA       = 0;
   targetDEC      = 0;
   currentRA      = 0;
   currentDEC     = 0;
   currentSet     = 0;
   UTCOffset      = 0;
   lastMove[0] = lastMove[1] = lastMove[2] = lastMove[3] = 0;

   localTM = new tm;
   
   utp->tm_mon  += 1;
   utp->tm_year += 1900;
   JD = UTtoJD(utp);
   
   IDLog("Julian Day is %g\n", JD);
   
   // Children call parent routines, this is the default
   IDLog("initilizaing from generic LX200 device...\n");
   IDLog("INDI Version: 2004-02-17\n");
 
   //enableSimulation(true);  
}

void LX200Generic::setCurrentDeviceName(const char * devName)
{
  strcpy(thisDevice, devName);

}

void LX200Generic::ISGetProperties(const char *dev)
{

 if (dev && strcmp (thisDevice, dev))
    return;

  // COMM_GROUP
  IDDefSwitch (&PowerSP, NULL);
  IDDefText   (&Port, NULL);
  IDDefSwitch (&AlignmentSw, NULL);

  // BASIC_GROUP
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
  IDDefNumber(&FocusSpeedNP, NULL);
  IDDefSwitch(&FocusMotionSw, NULL);
  IDDefNumber(&FocusTimerNP, NULL);

  // DATETIME_GROUP
  IDDefText   (&Time, NULL);
  IDDefNumber (&SDTime, NULL);

  // SITE_GROUP
  IDDefSwitch (&SitesSw, NULL);
  IDDefText   (&SiteName, NULL);
  IDDefNumber (&geoNum, NULL);
  
  /* Send the basic data to the new client if the previous client(s) are already connected. */		
   if (PowerSP.s == IPS_OK)
       getBasicData();

}

void LX200Generic::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
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
		ltp->tm_mon  += 1;
		ltp->tm_year += 1900;

	        
                /*dayDiff = utm.tm_mday - ltp->tm_mday;
		if (dayDiff == 0)
		   UTCOffset = (ltp->tm_hour - utm.tm_hour);  
		else if (dayDiff > 0)
		   UTCOffset = ltp->tm_hour - utm.tm_hour - 24;
		else UTCOffset = ltp->tm_hour - utm.tm_hour + 24;*/
		tzset();
		
		UTCOffset = timezoneOffset();
		
		IDLog("local time is %02d:%02d:%02d\nUTCOffset: %g\n", ltp->tm_hour, ltp->tm_min, ltp->tm_sec, UTCOffset);
		
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

                utm.tm_mon  += 1;
		utm.tm_year += 1900;

		IDLog("New JD is %f\n", (float) JD);

		if ((localTM->tm_mday == ltp->tm_mday ) && (localTM->tm_mon == ltp->tm_mon) &&
		    (localTM->tm_year == ltp->tm_year))
		{
		  IDSetText(&Time , "Time updated to %s", texts[0]);
		  return;
		}

		localTM = ltp;
		
		if (!strcmp(dev, "LX200 GPS"))
		{
			if ( ( err = setCalenderDate(utm.tm_mday, utm.tm_mon, utm.tm_year) < 0) )
	  		{
		  		handleError(&Time, err, "Setting UTC date.");
		  		return;
			}
		}
		else
		{
			if ( ( err = setCalenderDate(ltp->tm_mday, ltp->tm_mon, ltp->tm_year) < 0) )
	  		{
		  		handleError(&Time, err, "Setting local date.");
		  		return;
			}
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
	   char RAStr[32], DecStr[32];

	   fs_sexa(RAStr, newRA, 2, 3600);
	   fs_sexa(DecStr, newDEC, 2, 3600);
	  
	   IDLog("We received J2000 RA %g - DEC %g\n", newRA, newDEC);
	   IDLog("We received J2000 RA %s - DEC %s\n", RAStr, DecStr);
	   
	   /*apparentCoord( (double) J2000, JD, &newRA, &newDEC);
	   
	   fs_sexa(RAStr, newRA, 2, 3600);
	   fs_sexa(DecStr, newDEC, 2, 3600);
	   
	   IDLog("Processed to JNow RA %f - DEC %f\n", newRA, newDEC);
	   IDLog("Processed to JNow RA %s - DEC %s\n", RAStr, DecStr);*/

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
		snprintf (msg, sizeof(msg), "Site location updated to Lat %.32s - Long %.32s", l, L);
	    } else
	    {
		geoNum.s = IPS_IDLE;
		strcpy(msg, "Lat or Long missing or invalid");
	    }
	    IDSetNumber (&geoNum, "%s", msg);
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
	
	if (!strcmp(name, FocusTimerNP.name))
	{
	  if (checkPower(&FocusTimerNP))
	   return;
	   
	  // Don't update if busy
	  if (FocusTimerNP.s == IPS_BUSY)
	   return;
	   
	  IUUpdateNumbers(&FocusTimerNP, values, names, n);
	  
	  FocusTimerNP.s = IPS_OK;
	  
	  IDSetNumber(&FocusTimerNP, NULL);
	  IDLog("Setting focus timer to %g\n", FocusTimerN[0].value);
	  
	  return;

	}

        // Focus speed
	if (!strcmp (name, FocusSpeedNP.name))
	{
	  if (checkPower(&FocusSpeedNP))
	   return;

	  if (IUUpdateNumbers(&FocusSpeedNP, values, names, n) < 0)
		return;

	  /* disable timer and motion */
	  if (FocusSpeedN[0].value == 0)
	  {
	    IUResetSwitches(&FocusMotionSw);
	    FocusMotionSw.s = IPS_IDLE;
	    FocusTimerNP.s  = IPS_IDLE;
	    IDSetSwitch(&FocusMotionSw, NULL);
	    IDSetNumber(&FocusTimerNP, NULL);
	  }
	    
	  setFocuserSpeedMode( ( (int) FocusSpeedN[0].value));
	  FocusSpeedNP.s = IPS_OK;
	  IDSetNumber(&FocusSpeedNP, NULL);
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

	//IDLog("The device name is %s\n", dev);
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
	        abortSlewSw.s = IPS_OK;
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

	  if (dd > 0) geoNum.np[0].value = dd + mm / 60.0;
	  else geoNum.np[0].value = dd - mm / 60.0;
	  
	  if ( ( err = getSiteLongitude(&dd, &mm) < 0))
	  {
	        handleError(&SitesSw, err, "Selecting sites");
		return;
	  }
	  
	  if (dd > 0) geoNum.np[1].value = 360.0 - (dd + mm / 60.0);
	  else geoNum.np[1].value = (dd - mm / 60.0) * -1.0;
	  
	  getSiteName( SiteName.tp[0].text, currentSiteNum);

	  IDLog("Selecting site %d\n", currentSiteNum);
	  
	  geoNum.s = SiteName.s = SitesSw.s = IPS_OK;

	  IDSetNumber (&geoNum, NULL);
	  IDSetText   (&SiteName, NULL);
          IDSetSwitch (&SitesSw, NULL);
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
	  if (FocusTimerN[0].value > 0)  
	     FocusTimerNP.s  = IPS_BUSY;
	  
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
  if (simulation) return true;
  
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
	/*static int okCounter = 3;*/
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
	      /* Don't set current to target. This might leave residual cumulative error 
		currentRA = targetRA;
		currentDEC = targetDEC;
	      */
		
	       eqNum.np[0].value = lastRA  = currentRA;
	       eqNum.np[1].value = lastDEC = currentDEC;
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
	    {
		eqNum.np[0].value = currentRA;
		eqNum.np[1].value = currentDEC;
		IDSetNumber (&eqNum, NULL);
	    }
	    break;

	case IPS_OK:
	  
	  /*if (--okCounter >= 0)
	    break;
	  
	// Activate again in 3 seconds
	  okCounter = 3;*/
	  
	if ( (err = getLX200RA(&currentRA)) < 0 || (err = getLX200DEC(&currentDEC)) < 0)
	{
	  handleError(&eqNum, err, "Getting RA/DEC");
	  return;
	}
	
	if (fault)
	  correctFault();
	
	if ( (currentRA != lastRA) || (currentDEC != lastDEC))
	{
	  	eqNum.np[0].value = lastRA  = currentRA;
		eqNum.np[1].value = lastDEC = currentDEC;
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
	   /*apparentCoord( JD, (double) J2000, &currentRA, &currentDEC);*/
	   eqNum.np[0].value = currentRA;
	   eqNum.np[1].value = currentDEC;

	   IDSetNumber (&eqNum, NULL);
	     break;
	 case IPS_OK:
	   break;
	 case IPS_ALERT:
	   break;
	 }
	 
	 switch (FocusTimerNP.s)
	 {
	   case IPS_IDLE:
	     break;
	     
	   case IPS_BUSY:
	    FocusTimerN[0].value--;
	    
	    if (FocusTimerN[0].value == 0)
	    {
	      
	      if ( ( err = setFocuserSpeedMode(0) < 0) )
              {
	        handleError(&FocusSpeedNP, err, "setting focuser speed mode");
                IDLog("Error setting focuser speed mode\n");
                return;
	      } 
         
	      
	      FocusMotionSw.s = IPS_IDLE;
	      FocusTimerNP.s  = IPS_OK;
	      FocusSpeedNP.s  = IPS_OK;
	      
              IUResetSwitches(&FocusMotionSw);
	      FocusSpeedN[0].value = 0;
	      
	      IDSetNumber(&FocusSpeedNP, NULL);
	      IDSetSwitch(&FocusMotionSw, NULL);
	    }
	    
	    IDSetNumber(&FocusTimerNP, NULL);
	    break;
	    
	   case IPS_OK:
	    break;
	    
	   case IPS_ALERT:
	    break;
	 }
    

}

void LX200Generic::getBasicData()
{

  int err;
  struct tm *timep;
  time_t ut;
  time (&ut);
  timep = gmtime (&ut);
  strftime (Time.tp[0].text, sizeof(Time.tp[0].text), "%Y-%m-%dT%H:%M:%S", timep);

  IDLog("PC UTC time is %s\n", Time.tp[0].text);

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
  
  SiteNameT[0].text = new char[64];
  
  if ( (err = getSiteName(SiteNameT[0].text, currentSiteNum)) < 0)
    IDMessage(thisDevice, "Failed to get site name from device");
  else
    IDSetText   (&SiteName, NULL);
  
  if ( (err = getTrackFreq(&TrackFreq[0].value)) < 0)
     IDMessage(thisDevice, "Failed to get tracking frequency from device.");
  else
     IDSetNumber (&TrackingFreq, NULL);
     
     
  updateLocation();
  updateTime();
  
}

int LX200Generic::handleCoordSet()
{

  int  err;
  char syncString[256];
  char RAStr[32], DecStr[32];
  double dx, dy;
  
  IDLog("In Handle Coord Set()\n");

  switch (currentSet)
  {

    // Slew
    case LX200_SLEW:
          lastSet = LX200_SLEW;
	  if (eqNum.s == IPS_BUSY)
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

	  eqNum.s = IPS_BUSY;
	  fs_sexa(RAStr, targetRA, 2, 3600);
	  fs_sexa(DecStr, targetDEC, 2, 3600);
	  IDSetNumber(&eqNum, "Slewing to JNow RA %s - DEC %s", RAStr, DecStr);
	  IDLog("Slewing to JNow RA %s - DEC %s\n", RAStr, DecStr);
	  break;

     // Track
     case LX200_TRACK:
          IDLog("We're in LX200_TRACK\n");
          if (eqNum.s == IPS_BUSY)
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
		eqNum.s = IPS_BUSY;
		IDSetNumber(&eqNum, "Slewing to JNow RA %s - DEC %s", RAStr, DecStr);
		IDLog("Slewing to JNow RA %s - DEC %s\n", RAStr, DecStr);
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
  if (simulation) return 0;
  
  if (PowerSP.s != IPS_OK)
  {
    if (!strcmp(sp->label, ""))
    	IDMessage (thisDevice, "Cannot change property %s while the telescope is offline.", sp->name);
    else
        IDMessage (thisDevice, "Cannot change property %s while the telescope is offline.", sp->label);
	
    sp->s = IPS_IDLE;
    IDSetSwitch(sp, NULL);
    return -1;
  }

  return 0;
}

int LX200Generic::checkPower(INumberVectorProperty *np)
{
  if (simulation) return 0;
  
  if (PowerSP.s != IPS_OK)
  {
    
    if (!strcmp(np->label, ""))
    	IDMessage (thisDevice, "Cannot change property %s while the telescope is offline.", np->name);
    else
        IDMessage (thisDevice, "Cannot change property %s while the telescope is offline.", np->label);
	
    np->s = IPS_IDLE;
    IDSetNumber(np, NULL);
    return -1;
  }

  return 0;

}

int LX200Generic::checkPower(ITextVectorProperty *tp)
{

  if (simulation) return 0;
  
  if (PowerSP.s != IPS_OK)
  {
    if (!strcmp(tp->label, ""))
    	IDMessage (thisDevice, "Cannot change property %s while the telescope is offline.", tp->name);
    else
        IDMessage (thisDevice, "Cannot change property %s while the telescope is offline.", tp->label);
	
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
	
        if (simulation)
	{
	  PowerSP.s = IPS_OK;
	  IDSetSwitch (&PowerSP, "Simulated telescope is online.");
	  updateTime();
	  return;
	}
	
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

   signed char align = ACK();
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

void LX200Generic::enableSimulation(bool enable)
{
   simulation = enable;
   
   if (simulation)
     IDLog("Warning: Simulation is activated.\n");
   else
     IDLog("Simulation is disabled.\n");
}

void LX200Generic::updateTime()
{

  char cdate[32];
  double ctime;
  int h, m, s;
  int day, month, year, result;
  int UTC_h, UTC_month, UTC_year, UTC_day, daysInFeb;
  bool leapYear;
  
  tzset();
  
  UTCOffset = timezoneOffset();
  IDLog("Daylight: %s - TimeZone: %g\n", daylight ? "Yes" : "No", UTCOffset);
  
	
  if (simulation)
  {
    sprintf(UTC[0].text, "%d-%02d-%02dT%02d:%02d:%02d", 1979, 6, 25, 3, 30, 30);
    IDLog("Telescope ISO date and time: %s\n", UTC[0].text);
    IDSetText(&Time, NULL);
    return;
  }
  
  getLocalTime24(&ctime);
  getSexComponents(ctime, &h, &m, &s);
  
  UTC_h = h;
  
  if ( (result = getSDTime(&STime[0].value)) < 0)
    IDMessage(thisDevice, "Failed to retrieve siderial time from device.");
  
  getCalenderDate(cdate);
  
  result = sscanf(cdate, "%d/%d/%d", &year, &month, &day);
  if (result != 3) return;
  
  if (year % 4 == 0)
  {
    if (year % 100 == 0)
    {
      if (year % 400 == 0)
       leapYear = true;
      else 
       leapYear = false;
    }
    else
       leapYear = true;
  }
  else
       leapYear = false;
  
  daysInFeb = leapYear ? 29 : 28;
  
  UTC_year  = year; 
  UTC_month = month;
  UTC_day   = day;
  
  IDLog("day: %d - month %d - year: %d\n", day, month, year);
  
  // we'll have to convert telescope time to UTC manually starting from hour up
  // seems like a stupid way to do it.. oh well
  UTC_h = (int) UTCOffset + h;
  if (UTC_h < 0)
  {
   UTC_h += 24;
   UTC_day--;
  }
  else if (UTC_h > 24)
  {
   UTC_h -= 24;
   UTC_day++;
  }
  
  switch (UTC_month)
  {
    case 1:
    case 8:
     if (UTC_day < 1)
     {
     	UTC_day = 31;
     	UTC_month--;
     }
     else if (UTC_day > 31)
     {
       UTC_day = 1;
       UTC_month++;
     }
     break;
     
   case 2:
   if (UTC_day < 1)
     {
     	UTC_day = 31;
     	UTC_month--;
     }
     else if (UTC_day > daysInFeb)
     {
       UTC_day = 1;
       UTC_month++;
     }
     break;
     
  case 3:
     if (UTC_day < 1)
     {
     	UTC_day = daysInFeb;
     	UTC_month--;
     }
     else if (UTC_day > 31)
     {
       UTC_day = 1;
       UTC_month++;
     }
     break;
   
   case 4:
   case 6:
   case 9:
   case 11:
    if (UTC_day < 1)
     {
     	UTC_day = 31;
     	UTC_month--;
     }
     else if (UTC_day > 30)
     {
       UTC_day = 1;
       UTC_month++;
     }
     break;
   
   case 5:
   case 7:
   case 10:
   case 12:
    if (UTC_day < 1)
     {
     	UTC_day = 30;
     	UTC_month--;
     }
     else if (UTC_day > 31)
     {
       UTC_day = 1;
       UTC_month++;
     }
     break;
   
  }   
       
  if (UTC_month < 1)
  {
   UTC_month = 12;
   UTC_year--;
  }
  else if (UTC_month > 12)
  {
    UTC_month = 1;
    UTC_year++;
  }
  
  /* Format it into ISO 8601 */
  sprintf(UTC[0].text, "%d-%02d-%02dT%02d:%02d:%02d", UTC_year, UTC_month, UTC_day, UTC_h, m, s);
  
  IDLog("Local telescope time: %02d:%02d:%02d\n", h, m , s);
  IDLog("Telescope SD Time is: %g\n", STime[0].value);
  IDLog("UTC date and time: %s\n", UTC[0].text);
  

  // Let's send everything to the client
  IDSetText(&Time, NULL);
  IDSetNumber(&SDTime, NULL);

}

void LX200Generic::updateLocation()
{

 int dd = 0, mm = 0, err = 0;
 
 if ( (err = getSiteLatitude(&dd, &mm)) < 0)
    IDMessage(thisDevice, "Failed to get site latitude from device.");
  else
  {
    if (dd > 0)
    	geoNum.np[0].value = dd + mm/60.0;
    else
        geoNum.np[0].value = dd - mm/60.0;
  
      IDLog("Autostar Latitude: %d:%d\n", dd, mm);
      IDLog("INDI Latitude: %g\n", geoNum.np[0].value);
  }
  
  if ( (err = getSiteLongitude(&dd, &mm)) < 0)
    IDMessage(thisDevice, "Failed to get site longitude from device.");
  else
  {
    if (dd > 0) geoNum.np[1].value = 360.0 - (dd + mm/60.0);
    else geoNum.np[1].value = (dd - mm/60.0) * -1.0;
    
    IDLog("Autostar Longitude: %d:%d\n", dd, mm);
    IDLog("INDI Longitude: %g\n", geoNum.np[1].value);
  }
  
  IDSetNumber (&geoNum, NULL);

}


