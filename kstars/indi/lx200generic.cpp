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
char mydev[] = "LX200 Generic";

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

static void ISPoll(void *);

/*INDI controls */
static ISwitch PowerS[]          = {{"CONNECT" , "Connect" , ISS_OFF},{"DISCONNECT", "Disconnect", ISS_ON}};
static ISwitch AlignmentS []     = {{"Polar", "", ISS_ON}, {"AltAz", "", ISS_OFF}, {"Land", "", ISS_OFF}};
static ISwitch SitesS[]          = {{"Site 1", "", ISS_ON}, {"Site 2", "", ISS_OFF},  {"Site 3", "", ISS_OFF},  {"Site 4", "", ISS_OFF}};
static ISwitch SlewModeS[]       = {{"Max", "", ISS_ON}, {"Find", "", ISS_OFF}, {"Centering", "", ISS_OFF}, {"Guide", "", ISS_OFF}};
static ISwitch OnCoordSetS[]     = {{"Slew", "", ISS_ON }, {"Track", "", ISS_OFF}, {"Sync", "", ISS_OFF }};
static ISwitch TrackModeS[]      = {{ "Default", "", ISS_ON} , { "Lunar", "", ISS_OFF}, {"Manual", "", ISS_OFF}};
static ISwitch abortSlewS[]      = {{"Abort", "Abort Slew/Track", ISS_OFF }};

static ISwitch MovementS[]       = {{"N", "North", ISS_OFF}, {"W", "West", ISS_OFF}, {"E", "East", ISS_OFF}, {"S", "South", ISS_OFF}};
static ISwitch haltMoveS[]       = {{"TN", "Northward", ISS_OFF}, {"TW", "Westward", ISS_OFF}, {"TE", "Eastward", ISS_OFF}, {"TS", "Southward", ISS_OFF}};

static ISwitch	FocusSpeedS[]	 = { {"Halt", "", ISS_ON}, { "Fast", "", ISS_OFF }, {"Slow", "", ISS_OFF}};
static ISwitch  FocusMotionS[]	 = { {"Focus in", "", ISS_OFF}, {"Focus out", "", ISS_OFF}};

/* equatorial position */
static INumber eq[] = {
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
static ISwitchVectorProperty PowerSw	= { mydev, "CONNECTION" , "Connection", COMM_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, PowerS, NARRAY(PowerS)};
static IText PortT[]			= {{"Port", "Port", "/dev/ttyS0"}};
static ITextVectorProperty Port		= { mydev, "Ports", "Ports", COMM_GROUP, IP_RW, 0, IPS_IDLE, PortT, NARRAY(PortT)};

/* Basic data group */
static ISwitchVectorProperty AlignmentSw	= { mydev, "Alignment", "", COMM_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, AlignmentS, NARRAY(AlignmentS)};

static INumber altLimit[] = {
       {"minAlt", "min Alt", "%+03f", -90., 90., 0., 0.},
       {"maxAlt", "max Alt", "%+03f", -90., 90., 0., 0.}};
static INumberVectorProperty elevationLimit = { mydev, "altLimit", "Slew elevation Limit", BASIC_GROUP, IP_RW, 0, IPS_IDLE, altLimit, NARRAY(altLimit)};

/* Movement group */
static ISwitchVectorProperty OnCoordSetSw    = { mydev, "ON_COORD_SET", "On Set", BASIC_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, OnCoordSetS, NARRAY(OnCoordSetS)};
static ISwitchVectorProperty abortSlewSw     = { mydev, "ABORT_MOTION", "Abort", BASIC_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, abortSlewS, NARRAY(abortSlewS)};
static ISwitchVectorProperty SlewModeSw      = { mydev, "Slew rate", "", MOVE_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, SlewModeS, NARRAY(SlewModeS)};

static INumber MaxSlew[] = {{"maxSlew", "Max slew rate", "%g", 2.0, 9.0, 1.0, 9.}};
static INumberVectorProperty MaxSlewRate = { mydev, "Max slew Rate", "", MOVE_GROUP, IP_RW, 0, IPS_IDLE, MaxSlew, NARRAY(MaxSlew)};

static ISwitchVectorProperty TrackModeSw  = { mydev, "Tracking Mode", "", MOVE_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, TrackModeS, NARRAY(TrackModeS)};
static INumber TrackFreq[]  = {{ "trackFreq", "Tracking frequency", "%g", 50.0, 80.0, 0.1, 60.1}};
static INumberVectorProperty TrackingFreq= { mydev, "Tracking Frequency", "", MOVE_GROUP, IP_RW, 0, IPS_IDLE, TrackFreq, NARRAY(TrackFreq)};

static ISwitchVectorProperty MovementSw      = { mydev, "Move toward", "", MOVE_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, MovementS, NARRAY(MovementS)};
static ISwitchVectorProperty haltMoveSw      = { mydev, "Halt movement", "", MOVE_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, haltMoveS, NARRAY(haltMoveS)};

// Focus Control
static ISwitchVectorProperty	FocusSpeedSw  = {mydev, "Speed", "", FOCUS_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, FocusSpeedS, NARRAY(FocusSpeedS)};

static ISwitchVectorProperty	FocusMotionSw = {mydev, "Motion", "", FOCUS_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, FocusMotionS, NARRAY(FocusMotionS)};

/* Data & Time */
static IText UTC[] = {{"UTC", "UTC", "YYYY-MM-DDTHH:MM:SS"}};
ITextVectorProperty Time = { mydev, "TIME", "UTC Time", DATETIME_GROUP, IP_RW, 0, IPS_IDLE, UTC, NARRAY(UTC)};
//static INumber utcOffset[] = {"utcOffset", "UTC Offset", -12.0, 12.0, 0.5, 0.};
//static INumberVectorProperty UTCOffset = { mydev, "UTC", "", DATETIME_GROUP, 0, IPS_IDLE, utcOffset, NARRAY(utcOffset)};
static INumber STime[] = {{"SDTime", "Sidereal time", "%10.6m" , 0.,0.,0.,0.}};
static INumberVectorProperty SDTime = { mydev, "Sidereal Time", "", DATETIME_GROUP, IP_RW, 0, IPS_IDLE, STime, NARRAY(STime)};


/* Site managment */
static ISwitchVectorProperty SitesSw  = { mydev, "Sites", "", SITE_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, SitesS, NARRAY(SitesS)};
/* geographic location */
static INumber geo[] = {
    {"LAT",  "Latitude  D:M:S +N", "%10.6m",  -90.,  90., 0., 0.},
    {"LONG", "Longitude D:M:S +E", "%10.6m", 0., 360., 0., 0.},
};
static INumberVectorProperty geoNum = {
    mydev, "GEOGRAPHIC_COORD", "Geographic Location", SITE_GROUP, IP_RW, 0., IPS_IDLE,
    geo, NARRAY(geo),
};
static IText   SiteNameT[] = {{"SiteName", "", ""}};
static ITextVectorProperty SiteName = { mydev, "Site Name", "", SITE_GROUP, IP_RW, 0 , IPS_IDLE, SiteNameT, NARRAY(SiteNameT)};

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
     strcpy(mydev, "LX200 Classic");
     // 2. device = sub_class
     telescope = new LX200Classic();

     MaxReticleFlashRate = 3;
  }

  else if (strstr(me, "lx200gps"))
  {
     fprintf(stderr , "initilizaing from LX200 GPS device...\n");
     // 1. mydev = device_name
     strcpy(mydev, "LX200 GPS");
     // 2. device = sub_class
     telescope = new LX200GPS();

     MaxReticleFlashRate = 9;
  }
  else if (strstr(me, "lx200_16"))
  {

    IDLog("Initilizaing from LX200 16 device...\n");
    // 1. mydev = device_name
    strcpy(mydev, "LX200 16");
    // 2. device = sub_class
   telescope = new LX200_16();

   MaxReticleFlashRate = 3;
 }
 else if (strstr(me, "lx200autostar"))
 {
   fprintf(stderr , "initilizaing from autostar device...\n");

   // 1. mydev = device_name
   strcpy(mydev, "LX200 Autostar");
   // 2. device = sub_class
   telescope = new LX200Autostar();

   MaxReticleFlashRate = 9;
 }
 // be nice and give them a generic device
 else telescope = new LX200Generic();

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
   currentSiteNum = 1;
   currentCatalog = LX200_STAR_C;
   currentSubCatalog = 0;
   trackingMode = LX200_TRACK_DEFAULT;

   targetRA  = 0;
   targetDEC = 0;
   currentRA = 0;
   currentDEC= 0;
   lastSet   = 0;
   lastMove[0] = lastMove[1] = lastMove[2] = lastMove[3] = 0;

   localTM = new tm;
   JD = 0;

   // Children call parent routines, this is the default
   IDLog("initilizaing from generic LX200 device...\n");
}

bool LX200Generic::isTelescopeOn(void)
{
  return (PowerSw.sw[0].s == ISS_ON);
}

void LX200Generic::ISGetProperties(const char *dev)
{

 if (dev && strcmp (mydev, dev))
    return;

  // COMM_GROUP
  IDDefSwitch (&PowerSw);
  IDDefText   (&Port);
  IDDefSwitch (&AlignmentSw);

  // BASIC_GROUP
  IDDefNumber (&elevationLimit);
  IDDefNumber (&eqNum);
  IDDefSwitch (&OnCoordSetSw);
  IDDefSwitch (&abortSlewSw);

  // MOVE_GROUP
  IDDefNumber (&MaxSlewRate);
  IDDefSwitch (&SlewModeSw);
  IDDefSwitch (&TrackModeSw);
  IDDefNumber (&TrackingFreq);
  IDDefSwitch (&MovementSw);
  IDDefSwitch (&haltMoveSw);

  // FOCUS_GROUP
  IDDefSwitch(&FocusSpeedSw);
  IDDefSwitch(&FocusMotionSw);

  // DATETIME_GROUP
  IDDefText   (&Time);
  IDDefNumber (&SDTime);

  // SITE_GROUP
  IDDefSwitch (&SitesSw);
  IDDefText   (&SiteName);
  IDDefNumber (&geoNum);

  //TODO this is really a test message only
  IDMessage(NULL, "KTelescope components registered successfully");

}

void LX200Generic::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
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

	  if (setSiteName(texts[0], currentSiteNum))
	  {
	  	SiteName.s = IPS_OK;
		IDSetText( &SiteName , "Setting site name failed");
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
		setUTCOffset(UTCOffset);
	  	setLocalTime(ltp->tm_hour, ltp->tm_min, ltp->tm_sec);

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
		setCalenderDate(ltp->tm_mday, ltp->tm_mon, ltp->tm_year);
 		IDSetText(&Time , "Date changed, updating planetary data...");
	}
}


void LX200Generic::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
	int h =0, m =0, s=0;
	double newRA =0, newDEC =0;

	// ignore if not ours //
	if (strcmp (dev, mydev))
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
	   eqNum.s = IPS_BUSY;

	   IDLog("We recevined J2000 RA %f - DEC %f\n", newRA, newDEC);;
	   apparentCoord( (double) J2000, JD, &newRA, &newDEC);
	   IDLog("Processed to RA %f - DEC %f\n", newRA, newDEC);


	   if (!setObjectRA(newRA) && !setObjectDEC(newDEC))
	   {

               eqNum.s = IPS_BUSY;
	       eqNum.n[0].value = values[0];
	       eqNum.n[1].value = values[1];
	       targetRA  = newRA;
	       targetDEC = newDEC;

	       if (handleCoordSet())
	       {
	        eqNum.s = IPS_IDLE;
	    	IDSetNumber(&eqNum, NULL);
	       }
	    }
	    else
	    {
	        eqNum.s = IPS_ALERT;
		IDSetNumber (&eqNum, "Error setting coordinates.");
            }

	    return;

	   } // end nset
	   else
	   {
		eqNum.s = IPS_IDLE;
		IDSetNumber(&eqNum, "RA or Dec missing or invalid");
	    }

	    return;
	 }

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
	  setSDTime(h, m, s);
	  SDTime.n[0].value = values[0];
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
		setSiteLongitude(360.0 - newLong);
		setSiteLatitude(newLat);
		geoNum.n[0].value = newLat;
		geoNum.n[1].value = newLong;
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

	  if (!setTrackFreq(values[0]))
	  {
	    TrackingFreq.s = IPS_OK;
	    TrackingFreq.n[0].value = values[0];
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
	  else
	  {
            TrackingFreq.s = IPS_ALERT;
	    IDSetNumber(&TrackingFreq, "setting tracking frequency failed");
	  }
	  return;
	}

	if ( !strcmp (name, MaxSlewRate.name) )
	{

	 if (checkPower(&MaxSlewRate))
	  return;

	  setMaxSlewRate( (int) values[0]);
	  MaxSlewRate.s = IPS_OK;
	  MaxSlewRate.n[0].value = values[0];
	  IDSetNumber(&MaxSlewRate, NULL);
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
		setMinElevationLimit( (int) minAlt);
		setMaxElevationLimit( (int) maxAlt);
		elevationLimit.n[0].value = minAlt;
		elevationLimit.n[1].value = maxAlt;
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
	int dd, mm;

	// suppress warning
	names = names;

	//IDLog("in new Switch with Device= %s and Property= %s and #%d items\n", dev, name,n);
	//IDLog("SolarSw name is %s\n", SolarSw.name);

	// ignore if not ours //
	if (strcmp (dev, mydev))
	    return;

	// FIRST Switch ALWAYS for power
	if (!strcmp (name, PowerSw.name))
	{
   	 powerTelescope(states);
	 fprintf(stderr, "after powering telescope\n");
	 return;
	}

	if (!strcmp(name, OnCoordSetSw.name))
	{
  	  if (checkPower(&OnCoordSetSw))
	   return;

	  lastSet = getOnSwitch(states, n);
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

	    if (OnCoordSetSw.s == IPS_BUSY || OnCoordSetSw.s == IPS_OK)
	    {
	    	abortSlew();
		abortSlewSw.s = IPS_OK;
		abortSlewSw.sw[0].s = ISS_OFF;
		OnCoordSetSw.s = IPS_IDLE;
		eqNum.s = IPS_IDLE;
		IDSetSwitch(&abortSlewSw, "Slew aborted");
		IDSetSwitch(&OnCoordSetSw, NULL);
		IDSetNumber(&eqNum, NULL);

            }
	    else if (MovementSw.s == IPS_BUSY)
	    {
	        HaltMovement(LX200_NORTH);
		HaltMovement(LX200_WEST);
		HaltMovement(LX200_EAST);
		HaltMovement(LX200_SOUTH);
		lastMove[0] = lastMove[1] = lastMove[2] = lastMove[3] = 0;
		abortSlewSw.s = IPS_OK;
		abortSlewSw.sw[0].s = ISS_OFF;
		MovementSw.s = IPS_IDLE;
		resetSwitches(&MovementSw);
		eqNum.s = IPS_IDLE;
		IDSetSwitch(&abortSlewSw, "Slew aborted");
		IDSetSwitch(&MovementSw, NULL);
		IDSetNumber(&eqNum, NULL);
	    }
	    else
	    {
	        abortSlewSw.sw[0].s = ISS_OFF;
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

	  index = getOnSwitch(states, n);

	  resetSwitches(&AlignmentSw);
	  AlignmentSw.sw[index].s = ISS_ON;

	  setAlignmentMode(index);
	  AlignmentSw.s = IPS_OK;
          IDSetSwitch (&AlignmentSw, NULL);
	  return;

	}

        // Sites
	if (!strcmp (name, SitesSw.name))
	{
	  if (checkPower(&AlignmentSw))
	   return;

	  currentSiteNum = getOnSwitch(states, n) + 1;
	  resetSwitches(&SitesSw);
	  SitesSw.sw[currentSiteNum].s = ISS_ON;
	  IDLog("Selecting site %d\n", currentSiteNum);
	  selectSite(currentSiteNum);
	  getSiteLatitude(&dd, &mm);
	  geoNum.n[0].value = dd + mm / 60.0;
	  getSiteLongitude(&dd, &mm);
	  geoNum.n[1].value = dd + mm / 60.0;
	  getSiteName( SiteName.t[0].text, currentSiteNum);

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

	  index = getOnSwitch(states, n);
	  setFocuserSpeedMode(index);
          resetSwitches(&FocusSpeedSw);
	  FocusSpeedSw.sw[index].s = ISS_ON;

	  FocusSpeedSw.s = IPS_OK;
	  IDSetSwitch(&FocusSpeedSw, NULL);
	  return;
	}

	// Focus speed
	if (!strcmp (name, FocusMotionSw.name))
	{
	  if (checkPower(&FocusMotionSw))
	   return;

	  index = getOnSwitch(states, n);
	  setFocuserMotion(index);
          resetSwitches(&FocusMotionSw);

	  FocusMotionSw.s = IPS_OK;
	  IDSetSwitch(&FocusMotionSw, NULL);
	  return;
	}

	// Slew mode
	if (!strcmp (name, SlewModeSw.name))
	{
	  if (checkPower(&SlewModeSw))
	   return;

	  index = getOnSwitch(states, n);
	  setSlewMode(index);
          resetSwitches(&SlewModeSw);
	  SlewModeSw.sw[index].s = ISS_ON;

	  SlewModeSw.s = IPS_OK;
	  IDSetSwitch(&SlewModeSw, NULL);
	  return;
	}

	if (!strcmp (name, MovementSw.name))
	{
	  if (checkPower(&MovementSw))
	   return;

	 index = getOnSwitch(states, n);

	 if (index < 0)
	  return;

	  if (lastMove[index])
	    return;

	  lastMove[index] = 1;

	  MoveTo(index);

	  for (uint i=0; i < 4; i++)
	    MovementSw.sw[i].s = lastMove[i] == 0 ? ISS_OFF : ISS_ON;

	  MovementSw.s = IPS_BUSY;
	  IDSetSwitch(&MovementSw, "Moving %s...", Direction[index]);
	  return;
	}

	// Halt Movement
	if (!strcmp (name, haltMoveSw.name))
	{
	  if (checkPower(&haltMoveSw))
	   return;

	  index = getOnSwitch(states, n);

	  if (MovementSw.s == IPS_BUSY)
	  {
	  	HaltMovement(index);
		lastMove[index] = 0;

		if (!lastMove[0] && !lastMove[1] && !lastMove[2] && !lastMove[3])
		  MovementSw.s = IPS_IDLE;

		for (uint i=0; i < 4; i++)
		{
	    	   haltMoveSw.sw[i].s = ISS_OFF;
		   MovementSw.sw[i].s = lastMove[i] == 0 ? ISS_OFF : ISS_ON;
		}

		eqNum.s = IPS_IDLE;
		haltMoveSw.s = IPS_IDLE;

		IDSetSwitch(&haltMoveSw, "Moving toward %s aborted", Direction[index]);
	  	IDSetSwitch(&MovementSw, NULL);
	  }
	  else
	  {
	        haltMoveSw.sw[index].s = ISS_OFF;
	     	haltMoveSw.s = IPS_IDLE;
	        IDSetSwitch(&haltMoveSw, NULL);
	  }
	  return;
	 }

	// Tracking mode
	if (!strcmp (name, TrackModeSw.name))
	{
	  if (checkPower(&TrackModeSw))
	   return;

	  resetSwitches(&TrackModeSw);

	  trackingMode = getOnSwitch(states, n);
	  selectTrackingMode(trackingMode);

	  TrackModeSw.sw[trackingMode].s = ISS_ON;

	  TrackFreq[0].value = getTrackFreq();
	  TrackModeSw.s = IPS_OK;
	  IDSetNumber(&TrackingFreq, NULL);
	  IDSetSwitch(&TrackModeSw, NULL);
	  return;
	}

}

void LX200Generic::ISPoll()
{
        double dx, dy;

	if (!isTelescopeOn())
	 return;


	switch (OnCoordSetSw.s)
	{
	case IPS_IDLE:
	currentRA = getLX200RA();
	currentDEC = getLX200DEC();

        if ( fabs (currentRA - lastRA) > 0.01 || fabs (currentDEC - lastDEC) > 0.01)
	{
	        eqNum.n[0].value = currentRA;
		eqNum.n[1].value = currentDEC;
		//formatSex (currentRA, RA.nstr, XXYYZZ);
		//formatSex (currentDEC, DEC.nstr, SXXYYZZ);
		lastRA  = currentRA;
		lastDEC = currentDEC;
		IDSetNumber (&eqNum, NULL);

	}
        break;

        case IPS_BUSY:
	    currentRA = getLX200RA();
	    currentDEC = getLX200DEC();
	    dx = targetRA - currentRA;
	    dy = targetDEC - currentDEC;

	    IDLog("targetRA is %g, currentRA is %g\n", targetRA, currentRA);
	    IDLog("targetDEC is %g, currentDEC is %g\n*************************\n", targetDEC, currentDEC);

	    eqNum.n[0].value = currentRA;
	    eqNum.n[1].value = currentDEC;

	    // Wait until acknowledged or within threshold
	    if (fabs(dx) <= RA_THRESHOLD && fabs(dy) <= DEC_THRESHOLD)
	    {

		currentRA = targetRA;
		currentDEC = targetDEC;

		apparentCoord( JD, (double) J2000, &currentRA, &currentDEC);

		eqNum.n[0].value = currentRA;
		eqNum.n[1].value = currentDEC;
		OnCoordSetSw.s = IPS_OK;

		if (lastSet == 0)
		{

		  eqNum.s = IPS_OK;
		  resetSwitches(&OnCoordSetSw);
		  OnCoordSetSw.sw[0].s = ISS_ON;
		  IDSetSwitch (&OnCoordSetSw, "Slew is complete");
		}
		else
		{
		  eqNum.s = IPS_OK;
		  resetSwitches(&OnCoordSetSw);
		  OnCoordSetSw.sw[1].s = ISS_ON;
		  IDSetSwitch (&OnCoordSetSw, "Slew is complete. Tracking...");
		}

		 IDSetNumber (&eqNum, NULL);

	    } else
		IDSetNumber (&eqNum, NULL);
	    break;

	case IPS_OK:
	currentRA = getLX200RA();
	currentDEC = getLX200DEC();

        if ( fabs (currentRA - lastRA) > 0.01 || fabs (currentDEC - lastDEC) > 0.01)
	{
		//formatSex (currentRA, RA.nstr, XXYYZZ);
		//formatSex (currentDEC, DEC.nstr, SXXYYZZ);
		eqNum.n[0].value = currentRA;
		eqNum.n[1].value = currentDEC;
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
	     currentRA = getLX200RA();
	     currentDEC = getLX200DEC();

	     apparentCoord( JD, (double) J2000, &currentRA, &currentDEC);

	     eqNum.n[0].value = currentRA;
	     eqNum.n[1].value = currentDEC;

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

  struct tm *timep;
  time_t ut;
  time (&ut);
  timep = gmtime (&ut);
  strftime (Time.t[0].text, sizeof(Time.t[0].text), "%Y-%m-%dT%H:%M:%S", timep);

  IDLog("time is %s\n", Time.t[0].text);

  checkLX200Format();
  getAlignment();
  timeFormat = getTimeFormat() == 24 ? LX200_24 : LX200_AM;

  // We always do 24 hours
  if (timeFormat != LX200_24)
   toggleTimeFormat();

  targetRA  = getLX200RA();
  targetDEC = getLX200DEC();

  eqNum.n[0].value = targetRA;
  eqNum.n[1].value = targetDEC;

  //TODO check the Yahoo group for solution of this autostar specific read-problem
  //sprintf(minAlt.nstr, "%02d", getMinElevationLimit());
  //sprintf(maxAlt.nstr, "%02d", getMaxElevationLimit());

  STime[0].value = getSDTime();

  selectSite(currentSiteNum);
  getSiteLatitude(&dd, &mm);
  geoNum.n[0].value = dd + mm/60.0;
  getSiteLongitude(&dd, &mm);
  geoNum.n[1].value = dd + mm/60.0;

  SiteNameT[0].text = new char[64];
  getSiteName(SiteNameT[0].text, currentSiteNum);
  TrackFreq[0].value = getTrackFreq();

  IDSetNumber (&eqNum, NULL);
  IDSetNumber (&elevationLimit, NULL);
  IDSetNumber (&SDTime, NULL);
  IDSetNumber (&geoNum, NULL);
  IDSetNumber (&TrackingFreq, NULL);
  IDSetText   (&Time, NULL);
  IDSetText   (&SiteName, NULL);
  IDSetSwitch (&SitesSw, NULL);

}

int LX200Generic::handleCoordSet()
{

  int i=0;
  char syncString[256];
  char RAStr[32], DecStr[32];

  switch (lastSet)
  {

    // Slew & Track
    case 0:
	  if (OnCoordSetSw.s == IPS_BUSY)
	  {
	     abortSlew();

	     // sleep for 100 mseconds
	     usleep(100000);
	  }

	  if ((i = Slew()))
	  {
	    slewError(i);
	    return (-1);
	  }

	  OnCoordSetSw.s = IPS_BUSY;
	  eqNum.s = IPS_BUSY;
	  fs_sexa(RAStr, eqNum.n[0].value, 2, 3600);
	  fs_sexa(DecStr, eqNum.n[1].value, 2, 3600);
	  IDSetSwitch(&OnCoordSetSw, "Slewing to J2000 RA %s - DEC %s", RAStr, DecStr);
	  IDSetNumber(&eqNum, NULL);
	  break;

     case 1:
          if (OnCoordSetSw.s == IPS_BUSY)
	  {
	     abortSlew();

	     // sleep for 200 mseconds
	     usleep(200000);
	  }

	  if ( (fabs ( targetRA - currentRA ) >= TRACKING_THRESHOLD) ||
	       (fabs (targetDEC - currentDEC) >= TRACKING_THRESHOLD))
	  {

	        IDLog("Exceeded Tracking threshold, will attempt to slew to the new target.\n");
		IDLog("targetRA is %g, currentRA is %g\n", targetRA, currentRA);
	        IDLog("targetDEC is %g, currentDEC is %g\n*************************\n", targetDEC, currentDEC);

          	if ((i = Slew()))
	  	{
	    		slewError(i);
	    		return (-1);
	  	}

		fs_sexa(RAStr, eqNum.n[0].value, 2, 3600);
	        fs_sexa(DecStr, eqNum.n[1].value, 2, 3600);
		OnCoordSetSw.s = IPS_BUSY;
		IDSetNumber(&eqNum, NULL);
		IDSetSwitch(&OnCoordSetSw, "Slewing to J2000 RA %s - DEC %s", RAStr, DecStr);
	  }
	  else
	  {
	    IDLog("Tracking called, but tracking threshold not reached yet.\n");
	    eqNum.s = IPS_OK;
	    eqNum.n[0].value = currentRA;
	    eqNum.n[1].value = currentDEC;
	    OnCoordSetSw.s = IPS_OK;
            IDSetNumber(&eqNum, NULL);
	    IDSetSwitch(&OnCoordSetSw, NULL);
	  }
      break;

  // Sync
  case 2:

	  OnCoordSetSw.s = IPS_OK;
	  Sync(syncString);

	  resetSwitches(&OnCoordSetSw);
          OnCoordSetSw.sw[2].s = ISS_ON;
          eqNum.s = IPS_OK;
   	  IDSetNumber(&eqNum, NULL);
	  IDSetSwitch(&OnCoordSetSw, "Synchronization successful. %s", syncString);
	  break;
   }

   return (0);

}

void LX200Generic::resetSwitches(ISwitchVectorProperty *driverSw)
{

   for (int i=0; i < driverSw->nsw; i++)
      driverSw->sw[i].s = ISS_OFF;

}

int LX200Generic::getOnSwitch(ISState * states, int n)
{
 for (int i=0; i < n ; i++)
     if (states[i] == ISS_ON)
      return i;

 return -1;
}


int LX200Generic::checkPower(ISwitchVectorProperty *sp)
{
  if (PowerSw.s != IPS_OK)
  {
    IDMessage (mydev, "Cannot change a property while the telescope is offline.");
    sp->s = IPS_IDLE;
    IDSetSwitch(sp, NULL);
    return -1;
  }

  return 0;
}

int LX200Generic::checkPower(INumberVectorProperty *np)
{

  if (PowerSw.s != IPS_OK)
  {
    IDMessage (mydev, "Cannot change a property while the telescope is offline");
    np->s = IPS_IDLE;
    IDSetNumber(np, NULL);
    return -1;
  }

  return 0;

}

int LX200Generic::checkPower(ITextVectorProperty *tp)
{

  if (PowerSw.s != IPS_OK)
  {
    IDMessage (mydev, "Cannot change a property while the telescope is offline");
    tp->s = IPS_IDLE;
    IDSetText(tp, NULL);
    return -1;
  }

  return 0;

}

void LX200Generic::powerTelescope(ISState *s)
{
    for (uint i= 0; i < NARRAY(PowerS); i++)
     PowerS[i].s = s[i];

     switch (PowerSw.sw[0].s)
     {
      case ISS_ON:

         if (Connect(Port.t[0].text))
	 {
	   PowerS[0].s = ISS_OFF;
	   PowerS[1].s = ISS_ON;
	   IDSetSwitch (&PowerSw, "Error connecting to port %s\n", Port.t[0].text);
	   return;
	 }
	 if (testTelescope())
	 {
	   PowerS[0].s = ISS_OFF;
	   PowerS[1].s = ISS_ON;
	   IDSetSwitch (&PowerSw, "Error connecting to Telescope. Telescope is offline.");
	   return;
	 }

        IDLog("telescope test successfful\n");
	PowerSw.s = IPS_OK;
	IDSetSwitch (&PowerSw, "Telescope is online. Retrieving basic data...");
	getBasicData();
	break;

     case ISS_OFF:
         PowerS[0].s = ISS_OFF;
	 PowerS[1].s = ISS_ON;
         PowerSw.s = IPS_IDLE;
         IDSetSwitch (&PowerSw, "Telescope is offline.");
	 IDLog("Telescope is offline.");
	 Disconnect();
	 break;

    }

}

void LX200Generic::slewError(int slewCode)
{
    OnCoordSetSw.s = IPS_IDLE;

    if (slewCode == 1)
	IDSetSwitch (&OnCoordSetSw, "Object below horizon");
    else
	IDSetSwitch (&OnCoordSetSw, "Object below the minimum elevation limit");

}

void LX200Generic::getAlignment()
{

   if (PowerSw.s != IPS_OK)
    return;

   char align = ACK();
   if (align < 0)
   {
     IDSetSwitch (&AlignmentSw, "Retrieving alignment failed.");
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
