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

#include "lx200driver.h"
#include "lx200GPS.h"

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
#define SITE_GROUP	"Site Managment"
#define LIBRARY_GROUP	"Library"

void ISInit()
{

  if (!strcmp(me, "lx200gps") || !strcmp(me, "./lx200gps"))
  {
     fprintf(stderr , "initilizaing from LX200 GPS device...\n");
     // 1. mydev = device_name
     strcpy(mydev, "LX200 GPS");
     // 2. device = sub_class
     telescope = new LX200GPS();
    // and 9 again
   MaxReticleFlashRate = 9;
  }
  else if (!strcmp(me, "lx200_16") || !strcmp(me, "./lx200_16"))
  {

    fprintf(stderr , "initilizaing from LX200 16 device...\n");
    // Two important steps always performed when adding a sub-device
    // 1. mydev = device_name
    strcpy(mydev, "LX200 16");
    // 2. device = sub_class
   telescope = new LX200_16();

   // Back to 3 again for class 16"
   MaxReticleFlashRate = 3;
 }
 else if (!strcmp(me, "lx200autostar") || !strcmp(me, "./lx200autostar"))
 {
   fprintf(stderr , "initilizaing from autostar device...\n");

   // Two important steps always performed when adding a sub-device
   // 1. mydev = device_name
   strcpy(mydev, "LX200 Autostar");
   // 2. device = sub_class
   telescope = new LX200Autostar();
   // Misc. device-specific settings
   MaxReticleFlashRate = 9;
 }
 // be nice and give them a generic device
 else
   telescope = new LX200Generic();

}

/*INDI controls */
static INRange FreqRange         = { INR_MIN|INR_MAX|INR_STEP , 50.0, 80.0, 0.1};
//TODO
//static INRange RDutyCycle	 = { INR_MIN|INR_MAX|INR_STEP , 0, MaxReticleDutyCycle, 1};
static INRange RFlashRate	 = { INR_MIN|INR_MAX|INR_STEP , 0, MaxReticleFlashRate, 1};

static ISwitch PowerS[]          = {{"ON" , ISS_OFF},{"OFF", ISS_ON}};
static ISwitch PortS []         = {{"ttyS0", ISS_ON}, {"ttyS1", ISS_OFF}, {"ttyS2", ISS_OFF}, {"ttyS3", ISS_OFF} ,{"ttyUSB0" , ISS_OFF}, {"ttyUSB1", ISS_OFF}, {"ttyUSB2", ISS_OFF}, {"ttyUSB3", ISS_OFF}};
static ISwitch AlignmentS []     = {{"Polar", ISS_ON}, {"AltAz", ISS_OFF}, {"Land", ISS_OFF}};
static ISwitch SitesS[]          = {{"Site 1", ISS_ON}, {"Site 2", ISS_OFF},  {"Site 3", ISS_OFF},  {"Site 4", ISS_OFF}};
static ISwitch SlewModeS[]       = {{"Max", ISS_ON}, {"Find", ISS_OFF}, {"Centering", ISS_OFF}, {"Guide", ISS_OFF}};
static ISwitch OnCoordSetS[]     = {{ "Slew", ISS_ON }, { "Track", ISS_OFF }, { "Sync", ISS_OFF }};
static ISwitch TrackModeS[]      = {{ "Default", ISS_ON} , { "Lunar", ISS_OFF}, {"Manual", ISS_OFF}};
static ISwitch abortSlewS[]      = {{"Abort Slew/Track", ISS_OFF }};

static ISwitch MovementS[]       = {{"North", ISS_OFF}, {"West", ISS_OFF}, {"East", ISS_OFF}, {"South", ISS_OFF}};
static ISwitch haltMoveS[]       = {{"Northward", ISS_OFF}, {"Westward", ISS_OFF}, {"Eastward", ISS_OFF}, {"Southward", ISS_OFF}};

static ISwitch StarCatalogS[]    = {{"STAR", ISS_ON}, {"SAO", ISS_OFF}, {"GCVS", ISS_OFF}};
static ISwitch DeepSkyCatalogS[] = {{"NGC", ISS_ON}, {"IC", ISS_OFF}, {"UGC", ISS_OFF}, {"Caldwell", ISS_OFF}, {"Arp", ISS_OFF}, {"Abell", ISS_OFF}, {"Messier", ISS_OFF}};
static ISwitch SolarS[]          = {{"Mercury", ISS_OFF}, {"Venus", ISS_OFF}, {"Moon", ISS_OFF}, {"Mars", ISS_OFF}, {"Jupiter", ISS_OFF}, {"Saturn", ISS_OFF},
				    {"Uranus", ISS_OFF}, {"Neptune", ISS_OFF}, {"Pluto", ISS_OFF}};

/* Fundamental group */
static ISwitches PowerSw	 = { mydev, "Power" , PowerS, NARRAY(PowerS), ILS_IDLE, 0, COMM_GROUP };
static ISwitches PortSw          = { mydev, "Ports" , PortS, NARRAY(PortS), ILS_IDLE, 0, COMM_GROUP};

/* Basic data group */
static ISwitches AlignmentSw     = { mydev, "Alignment", AlignmentS, NARRAY(AlignmentS), ILS_IDLE, 0, BASIC_GROUP };
static INumber RA          = { mydev, "RA", NULL, ILS_IDLE, 0 , BASIC_GROUP};
static INumber DEC         = { mydev, "DEC", NULL, ILS_IDLE, 0 , BASIC_GROUP};
static IText   ObjectInfo        = { mydev, "Object Info", NULL, ILS_IDLE, 0 , BASIC_GROUP};

/* Movement group */
static ISwitches OnCoordSetSw    = { mydev, "OnCoordSet", OnCoordSetS, NARRAY(OnCoordSetS), ILS_IDLE, 0, BASIC_GROUP};
static ISwitches SlewModeSw      = { mydev, "Slew rate", SlewModeS, NARRAY(SlewModeS), ILS_IDLE, 0, MOVE_GROUP};
static ISwitches TrackModeSw     = { mydev, "Tracking Mode", TrackModeS, NARRAY(TrackModeS), ILS_IDLE, 0, MOVE_GROUP};
static INumber TrackingFreq      = { mydev, "Tracking Frequency", NULL, ILS_IDLE, 0, MOVE_GROUP};
static ISwitches abortSlewSw     = { mydev, "Abort Slew/Track", abortSlewS, NARRAY(abortSlewS), ILS_IDLE, 0, MOVE_GROUP};
static ISwitches MovementSw      = { mydev, "Move toward", MovementS, NARRAY(MovementS), ILS_IDLE, 0, MOVE_GROUP};
static ISwitches haltMoveSw      = { mydev, "Halt movement", haltMoveS, NARRAY(haltMoveS), ILS_IDLE, 0, MOVE_GROUP};

/* Data & Time */
static IText Time                = { mydev, "Time", NULL, ILS_IDLE, 0 , DATETIME_GROUP};
static INumber SDTime            = { mydev, "Sidereal Time", NULL, ILS_IDLE, 0, DATETIME_GROUP};
static INumber UTCOffset         = { mydev, "UTC Offset", NULL, ILS_IDLE, 0, DATETIME_GROUP};

/* Site managment */
static ISwitches SitesSw         = { mydev, "Sites", SitesS, NARRAY(SitesS), ILS_IDLE, 0, SITE_GROUP};
static IText   SiteName          = { mydev, "Site Name", NULL, ILS_IDLE, 0, SITE_GROUP};
static INumber SiteLong	         = { mydev, "Long", NULL, ILS_IDLE, 0, SITE_GROUP};
static INumber SiteLat	         = { mydev, "Lat", NULL, ILS_IDLE, 0, SITE_GROUP};

/* Library group */
static ISwitches StarCatalogSw   = { mydev, "Star Catalogs", StarCatalogS, NARRAY(StarCatalogS), ILS_IDLE, 0, LIBRARY_GROUP};
static ISwitches DeepSkyCatalogSw= { mydev, "Deep Sky Catalogs", DeepSkyCatalogS, NARRAY(DeepSkyCatalogS), ILS_IDLE, 0,  LIBRARY_GROUP};
static ISwitches SolarSw         = { mydev, "Solar System", SolarS, NARRAY(SolarS), ILS_IDLE, 0, LIBRARY_GROUP};
static INumber ObjectNo          = { mydev, "Object Number", NULL, ILS_IDLE, 0, LIBRARY_GROUP};

/* send client definitions of all properties */
void ISGetProperties (const char *dev) {telescope->ISGetProperties(dev);}
void ISNewText (IText *t) {telescope->ISNewText(t);}
void ISNewNumber (INumber *n) {telescope->ISNewNumber(n);}
void ISNewSwitch (ISwitches *s) {telescope->ISNewSwitch(s);}
void ISPoll () {telescope->ISPoll();}

/**************************************************
*** LX200 Generic Implementation
***************************************************/

LX200Generic::LX200Generic()
{
   ICPollMe (POLLMS);
   RA.nstr     = strcpy (new char[12] , " 00:00:00");
   DEC.nstr    = strcpy (new char[12] , " 00:00:00");
   UTCOffset.nstr    = strcpy (new char[4]  , "00");
   Time.text         = strcpy (new char[32]  , "YYYY-MM-DDTHH:MM:SS.SS");
   SDTime.nstr       = strcpy (new char[16]  , "HH:MM:SS");
   SiteLong.nstr     = strcpy (new char[9]  , "DDD:MM");
   SiteLat.nstr      = strcpy (new char[9]  , "DD:MM");
   ObjectNo.nstr     = strcpy (new char[5]  , "NNNN");
   TrackingFreq.nstr = strcpy (new char[4]  , "00.0");
   ObjectInfo.text   = new char[64];
   SiteName.text     = new char[16];

   currentSiteNum = 1;
   currentCatalog = LX200_STAR_C;
   currentSubCatalog = 0;
   trackingMode = LX200_TRACK_DEFAULT;

   targetRA  = 0;
   targetDEC = 0;
   portIndex = 0;
   lastSet   = 0;

   // Children call parent routines, this is the default
   fprintf(stderr , "initilizaing from generic LX200 device...\n");

}

void LX200Generic::ISGetProperties(const char *dev)
{

 if (dev && strcmp (mydev, dev))
    return;

  ICDefSwitches (&PowerSw, "Power", ISP_W, IR_1OFMANY);
  ICDefSwitches (&PortSw, "Port", ISP_W, IR_1OFMANY);

  ICDefSwitches (&AlignmentSw, "Alignment", ISP_W, IR_1OFMANY);
  ICDefNumber (&RA, "RA", IP_RW, NULL);
  ICDefNumber (&DEC, "DEC", IP_RW, NULL);
  ICDefSwitches (&OnCoordSetSw, "On Set", ISP_W, IR_1OFMANY);
  ICDefText   (&ObjectInfo, "Object Info", IP_RO);

  ICDefSwitches (&SlewModeSw, "Slew rate", ISP_W, IR_1OFMANY);
  ICDefSwitches (&TrackModeSw, "Tracking Mode", ISP_W, IR_1OFMANY);
  ICDefNumber   (&TrackingFreq, "Tracking Freq.", IP_RW, &FreqRange);
  ICDefSwitches (&abortSlewSw, "Abort", ISP_W, IR_1OFMANY);
  ICDefSwitches (&MovementSw, "Move toward", ISP_W, IR_1OFMANY);
  ICDefSwitches (&haltMoveSw, "Halt movement", ISP_W, IR_1OFMANY);

  ICDefText(&Time, "Time", IP_RW);
  ICDefNumber(&UTCOffset, "UTC Offset", IP_RW, NULL);
  ICDefNumber(&SDTime, "Sidereal Time", IP_RW, NULL);

  ICDefSwitches (&SitesSw, "Select site", ISP_W, IR_1OFMANY);
  ICDefText     (&SiteName, "Site name", IP_RW);
  ICDefNumber   (&SiteLong, "Site Longitude", IP_RW, NULL);
  ICDefNumber   (&SiteLat,  "Site Latitude", IP_RW, NULL);

  ICDefSwitches (&StarCatalogSw, "Star Catalogs", ISP_W, IR_1OFMANY);
  ICDefSwitches (&DeepSkyCatalogSw, "Deep Sky Catalogs", ISP_W, IR_1OFMANY);
  ICDefSwitches (&SolarSw, "Solar System", ISP_W, IR_1OFMANY);
  ICDefNumber   (&ObjectNo, "Object Number", IP_RW, NULL);

  //TODO this is really a test message only
  ICMessage(NULL, "KTelescope components registered successfully");

}

void LX200Generic::ISNewText(IText *t)
{
        int dd, mm, yy;
	int h ,  m,  s;
	char localTime[64];
  	char localDate[64];


	// ignore if not ours //
	if (strcmp (t->dev, mydev))
	    return;

	if ( !strcmp (t->name, SiteName.name) )
	{
	  if (checkPower())
	  {
	    SiteName.s = ILS_IDLE;
	    ICSetText(&SiteName, NULL);
	    return;
	  }

	  if (setSiteName(t->text, currentSiteNum))
	  {
	  	SiteName.s = ILS_OK;
		ICSetText( &SiteName , "Setting site name failed");
		return;
	  }
	     SiteName.s = ILS_OK;
   	     strcpy(SiteName.text, t->text);
	     ICSetText( &SiteName , "Site name updated");
	     return;

       }

       if ( !strcmp (t->name, Time.name) )
       {
	  if (checkPower())
	  {
	    Time.s = ILS_IDLE;
	    ICSetText(&Time, NULL);
	    return;
	  }

	  if (extractDateTime(t->text, localTime, localDate))
	  {
	    Time.s = ILS_IDLE;
	    ICSetText(&Time , "Time invalid");
	    return;
	  }

	  	// Time changed
	  	if (strcmp(lastTime, localTime))
	  	{

	  		if (extractTime(localTime, &h, &m, &s))
		  	{
	    		Time.s = ILS_IDLE;
	    		ICSetText(&Time , "Time invalid");
	    		return;
	  		}

	  		fprintf(stderr, "time is %02d:%02d:%02d\n", h, m, s);
	  		setLocalTime(h, m, s);
		  	Time.s = ILS_OK;

			sprintf(Time.text, "%02d:%02d:%02d", h, m , s);
  	  		ICSetText(&Time , "Time updated to %s", Time.text);

			strcpy(lastTime, localTime);
		}

		// Date changed
		if (strcmp(lastDate, localDate))
		{

			if (extractDate(localDate, &dd, &mm, &yy))
	  		{
	    		Time.s = ILS_IDLE;
	   		ICSetText(&Time , "Date invalid");
	   		return;
	  		}

	  		setCalenderDate(dd, mm, yy);
	  		Time.s = ILS_OK;
	  		ICSetText(&Time , "Date changed, updating planetary data...");

	  		strcpy (lastDate, localDate);
	        }
	 return;
        }

}


int LX200Generic::handleCoordSet()
{

  int i=0;
  char syncString[256];
  char RAbuffer[64];
  char Decbuffer[64];

  switch (lastSet)
  {
    // Slew
    case 0:

	  if (OnCoordSetSw.s == ILS_BUSY)
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

	  formatSex(targetRA, RAbuffer, XXYYZZ);
	  formatSex(targetDEC, Decbuffer, SXXYYZZ);
	  strcpy(RA.nstr, RAbuffer);
	  strcpy(DEC.nstr, Decbuffer);
	  OnCoordSetSw.s = ILS_BUSY;
	  ICSetNumber(&RA, NULL);
	  ICSetNumber(&DEC, NULL);
	  ICSetSwitch(&OnCoordSetSw, "Slewing to RA %s - DEC %s", RAbuffer, Decbuffer);
	  break;

   // Track
   case 1:

	  if (OnCoordSetSw.s == ILS_BUSY)
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

	  formatSex(targetRA, RAbuffer, XXYYZZ);
	  formatSex(targetDEC, Decbuffer, SXXYYZZ);
	  OnCoordSetSw.s = ILS_BUSY;
	  ICSetSwitch(&OnCoordSetSw, "Slewing to RA %s - DEC %s", RAbuffer, Decbuffer);
	  break;
  // Sync
  case 2:

	  OnCoordSetSw.s = ILS_OK;
	  Sync(syncString);

	  ICSetSwitch(&OnCoordSetSw, "Synchronization successful. %s", syncString);
	  break;
   }

   return (0);

}

void LX200Generic::ISNewNumber (INumber *n)
{
	float h =0, d=0, m =0, s=0;
	int hour, min, sec;
	int num;

	// ignore if not ours //
	if (strcmp (n->dev, mydev))
	    return;

	if ( !strcmp (n->name, RA.name) && !validateSex(n->nstr, &h, &m, &s ))
	{
	  if (h < 0 || h > 24)
	  {
	   RA.s = ILS_IDLE;
	   ICSetNumber (&RA, "RA coordinate out of range (0 to 24 hours)");
	   return;
	  }

	  if (checkPower())
	  {
	    RA.s = ILS_IDLE;
	    ICSetNumber(&RA, NULL);
	    return;
	  }

	   if (!setObjectRA( (int) h,(int) m, (int) s))
	   {
	       RA.s = ILS_OK;
	       getSex(n->nstr, &targetRA);
	       getObjectInfo(ObjectInfo.text);
	       ICSetText   (&ObjectInfo, NULL);
	       if (handleCoordSet())
	       {
	        RA.s = ILS_IDLE;
	    	ICSetNumber(&RA, NULL);
	       }

	       return;

	    }
	    else
	    {
	        RA.s = ILS_ALERT;
		ICSetNumber (&RA, "Error setting RA");
		return;
            }

	} // end RA set


	if ( !strcmp (n->name, DEC.name) && !validateSex(n->nstr, &d, &m, &s))
	{
	  if (d < -90.0 || d > 90.0)
	  {
	   DEC.s = ILS_IDLE;
	   ICSetNumber (&DEC, "DEC coordinate out of range (-90 to +90 degrees)");
	   return;
	  }

	  if (checkPower())
	  {
	    DEC.s = ILS_IDLE;
	    ICSetNumber(&DEC, NULL);
	    return;
	  }

	  if (!setObjectDEC((int) d, (int) m, (int) s))
	    {
	       DEC.s = ILS_OK;
	       getSex(n->nstr, &targetDEC);
	       getObjectInfo(ObjectInfo.text);
	       ICSetText   (&ObjectInfo, NULL);
	       if (handleCoordSet())
	       {
	        DEC.s = ILS_IDLE;
	        ICSetNumber(&DEC, NULL);
	       }

	       return;

	    }
	    else
	    {
	        DEC.s = ILS_ALERT;
		ICSetNumber (&DEC, "Error setting DEC");
		return;
            }
	} // end DEC set

	if ( !strcmp (n->name, UTCOffset.name))
	{
	  if (checkPower())
	  {
	    UTCOffset.s = ILS_IDLE;
	    ICSetNumber(&UTCOffset, NULL);
	    return;
	  }

	  strcpy (UTCOffset.nstr, n->nstr);

	  if (!setUTCOffset(atoi(UTCOffset.nstr)))
	    {
	       UTCOffset.s = ILS_OK;
	       ICSetNumber (&UTCOffset, NULL);
	       return;

	    }
	    else
	    {
	        UTCOffset.s = ILS_ALERT;
		ICSetNumber (&UTCOffset, "Error setting UTC Offset");
		return;
            }
	} // end UTC Offset

	if ( !strcmp (n->name, SDTime.name) )
	{
	  if (checkPower())
	  {
	    SDTime.s = ILS_IDLE;
	    ICSetNumber(&SDTime, NULL);
	    return;
	  }

	  if (extractTime(n->nstr, &hour, &min, &sec))
	  {
	    SDTime.s = ILS_IDLE;
	    ICSetNumber(&SDTime , "Time invalid");
	    return;
	  }

	  fprintf(stderr, "time is %02d:%02d:%02d\n", hour, min, sec);
	  setSDTime(hour,  min, sec);
	  SDTime.s = ILS_OK;
   	  strcpy(SDTime.nstr , n->nstr);
	  ICSetNumber(&SDTime , "Sidereal time updated to %02d:%02d:%02d", hour, min, sec);

	  return;
        }

	if ( !strcmp (n->name, SiteLong.name) )
	{
	  if (checkPower())
	  {
	    SiteLong.s = ILS_IDLE;
	    ICSetNumber(&SiteLong, NULL);
	    return;
	  }

          if (validateSex(n->nstr, &h, &m,&s))
	  {
            SiteLong.s = ILS_IDLE;
	    ICSetNumber(&SiteLong , NULL);
	    return;
	  }
	  if (m > 59 || m < 0 || h < 0 || h > 360)
	  {
 	    SiteLong.s = ILS_IDLE;
	    ICSetNumber(&SiteLong , "Coordinates invalid");
	    return;
	  }

	  setSiteLongitude((int) h, (int) m);
	  SiteLong.s = ILS_OK;
	  strcpy(SiteLong.nstr , n->nstr);
	  ICSetNumber(&SiteLong , "Site longitude updated to %d:%d", (int) h, (int) m);
	  return;
        }

	if ( !strcmp (n->name, SiteLat.name) )
	{
	  if (checkPower())
	  {
	    SiteLat.s = ILS_IDLE;
	    ICSetNumber(&SiteLat, NULL);
	    return;
	  }

	  if (validateSex(n->nstr, &d, &m, &s))
	  {
            SiteLong.s = ILS_IDLE;
	    ICSetNumber(&SiteLong , NULL);
	    return;
	  }
	  if (m > 59 || m < 0 || d < -90 || d > 90)
	  {
 	    SiteLong.s = ILS_IDLE;
	    ICSetNumber(&SiteLong , "Coordinates invalid");
	    return;
	  }


          setSiteLatitude( (int) d, (int) m);
	  SiteLat.s = ILS_OK;
	  strcpy(SiteLat.nstr , n->nstr);
	  ICSetNumber(&SiteLat , "Site latitude updated to %d:%d", (int) d, (int) m);
	  return;
        }

	if ( !strcmp (n->name, ObjectNo.name) )
	{
	  if (checkPower())
	  {
	    ObjectNo.s = ILS_IDLE;
	    ICSetNumber(&ObjectNo, NULL);
	    return;
	  }

	  if (!sscanf(n->nstr, "%d", &num) || num > 9999 || num < 0)
	  {
	    ObjectNo.s = ILS_IDLE;
	    ICSetNumber(&ObjectNo , "Range invalid");
	    return;
	  }

          selectCatalogObject( currentCatalog, num);
	  formatSex ( (targetRA = getObjectRA()), RA.nstr, XXYYZZ);
          formatSex ( (targetDEC = getObjectDEC()), DEC.nstr, SXXYYZZ);
	  ObjectNo.s = ILS_OK;
	  strcpy(ObjectNo.nstr , n->nstr);
	  getObjectInfo(ObjectInfo.text);
	  ICSetNumber(&ObjectNo , "Object updated");
	  ICSetNumber(&RA, NULL);
	  ICSetNumber(&DEC, NULL);
	  ICSetText  (&ObjectInfo, NULL);
	  return;
        }

	if ( !strcmp (n->name, TrackingFreq.name) )
	{

	 if (checkPower())
	  {
	    TrackingFreq.s = ILS_IDLE;
	    ICSetNumber(&TrackingFreq, NULL);
	    return;
	  }

	  sscanf(n->nstr, "%f", &h);

	  fprintf(stderr, "Trying to set track freq of: %f", h);

	  if (!setTrackFreq((double) h))
	  {
            strcpy(TrackingFreq.nstr, n->nstr);
	    TrackingFreq.s = ILS_OK;
	    ICSetNumber(&TrackingFreq, "Tracking frequency set to %04.1f", h);
	    if (trackingMode != LX200_TRACK_MANUAL)
	    {
	      trackingMode = LX200_TRACK_MANUAL;
	      TrackModeS[0].s = ISS_OFF;
	      TrackModeS[1].s = ISS_OFF;
	      TrackModeS[2].s = ISS_ON;
	      TrackModeSw.s   = ILS_OK;
	      selectTrackingMode(trackingMode);
	      ICSetSwitch(&TrackModeSw, NULL);
	    }
	    return;
	  }
	  else
	  {
            TrackingFreq.s = ILS_ALERT;
	    ICSetNumber(&TrackingFreq, "setting tracking frequency failed");
	  }
	  return;
	}


}

void LX200Generic::ISNewSwitch(ISwitches *s)
{

	int index[16];
	int dd, mm;
	static int nOfMovements = 0;

	// ignore if not ours //
	if (strcmp (s->dev, mydev))
	    return;

	// FIRST Switch ALWAYS for power
	if (!strcmp (s->name, PowerSw.name))
	{
   	 powerTelescope(s);
	 return;
	}

        if (!validateSwitch(s, &PortSw, NARRAY(PortS), index, 0))
	{
	  portIndex = index[0];
	  PortSw.s = ILS_OK;
	  ICSetSwitch (&PortSw, NULL);
	  return;
	}

	if (!validateSwitch(s, &OnCoordSetSw, NARRAY(OnCoordSetS), index, 0))
	{
	  lastSet = index[0];
	  OnCoordSetSw.s = ILS_OK;
	  ICSetSwitch(&OnCoordSetSw, NULL);
	  return;
	}

	// Abort Slew
	if (!strcmp (s->name, abortSlewSw.name))
	{
	  if (checkPower())
	  {
	    abortSlewSw.s = ILS_IDLE;
	    ICSetSwitch(&abortSlewSw, NULL);
	    return;
	  }

	    if (OnCoordSetSw.s == ILS_BUSY || OnCoordSetSw.s == ILS_OK)
	    {
	    	abortSlew();
		abortSlewSw.s = ILS_OK;
		OnCoordSetSw.s = ILS_IDLE;
		ICSetSwitch(&abortSlewSw, "Slew aborted");
		ICSetSwitch(&OnCoordSetSw, NULL);
            }
	    else if (MovementSw.s == ILS_BUSY)
	    {
	        HaltMovement(LX200_NORTH);
		HaltMovement(LX200_WEST);
		HaltMovement(LX200_EAST);
		HaltMovement(LX200_SOUTH);
		abortSlewSw.s = ILS_OK;
		MovementSw.s = ILS_IDLE;
		ICSetSwitch(&abortSlewSw, "Slew aborted");
		ICSetSwitch(&MovementSw, NULL);
	    }
	    else
	    {
	        abortSlewSw.s = ILS_IDLE;
	        ICSetSwitch(&abortSlewSw, NULL);
	    }

	    return;
	}

	// Alignment

	if (!validateSwitch(s, &AlignmentSw, NARRAY(AlignmentS), index, 1))
	{
	  setAlignmentMode(index[0]);
	  AlignmentSw.s = ILS_OK;
          ICSetSwitch (&AlignmentSw, NULL);
	  return;

	}

	if (!validateSwitch(s, &SitesSw, NARRAY(SitesS), index, 1))
	{
	  currentSiteNum = index[0] + 1;
	  fprintf(stderr, "Selecting site %d\n", currentSiteNum);
	  selectSite(currentSiteNum);
	  getSiteLongitude(&dd, &mm);
	  sprintf (SiteLong.nstr, "%03d:%02d", dd, mm);
	  getSiteLatitude(&dd, &mm);
	  sprintf (SiteLat.nstr, "%+03d:%02d", dd, mm);

	  getSiteName( SiteName.text, currentSiteNum);

	  SiteLong.s = ILS_OK;
	  SiteLat.s = ILS_OK;
	  SiteName.s = ILS_OK;
	  SitesSw.s = ILS_OK;

	  ICSetNumber (&SiteLong, NULL);
	  ICSetNumber (&SiteLat,  NULL);
	  ICSetText   (&SiteName, NULL);
          ICSetSwitch (&SitesSw, NULL);
	  return;
	}

	if (!validateSwitch(s, &SlewModeSw, NARRAY(SlewModeS), index, 1))
	{
	  setSlewMode(index[0]);
	  SlewModeSw.s = ILS_OK;
	  ICSetSwitch(&SlewModeSw, NULL);
	  return;
	}

	if (!validateSwitch(s, &MovementSw, NARRAY(MovementS), index,1))
	{
	  MoveTo(index[0]);
	  MovementSw.s = ILS_BUSY;
	  nOfMovements++;
	  ICSetSwitch(&MovementSw, "Moving %s...", LX200Direction[index[0]]);
	  //fprintf(stderr, "Moving %s...", LX200Direction[index[0]]);
	  return;
	}

	if (!validateSwitch(s, &haltMoveSw, NARRAY(haltMoveS), index, 1))
	{
	  if (MovementSw.s == ILS_BUSY)
	  {
	     //fprintf(stderr, "Halting movement in this direction %s\n",  LX200Direction[index[0]]);
	     //fprintf(stderr, "current number of movements
	  	HaltMovement(index[0]);
		nOfMovements--;

		if (nOfMovements <= 0)
		{
	  	  MovementSw.s = ILS_IDLE;
		  nOfMovements = 0;
		}

		MovementSw.sw[0].s = ISS_OFF;
		MovementSw.sw[1].s = ISS_OFF;
		MovementSw.sw[2].s = ISS_OFF;
		MovementSw.sw[3].s = ISS_OFF;
                haltMoveSw.s = ILS_IDLE;

		ICSetSwitch(&haltMoveSw, "Moving toward %s aborted", LX200Direction[index[0]]);
	  	ICSetSwitch(&MovementSw, NULL);
	  }
	  else
	  {
	     	haltMoveSw.s = ILS_IDLE;
	        ICSetSwitch(&haltMoveSw, NULL);
	  }
	  return;
	 }

	if (!validateSwitch(s, &StarCatalogSw, NARRAY(StarCatalogS), index,1))
	{
	   currentCatalog = LX200_STAR_C;

	  if (selectSubCatalog(currentCatalog, index[0]))
	  {
	   currentSubCatalog = index[0];
	   StarCatalogSw.s = ILS_OK;
	   ICSetSwitch(&StarCatalogSw, NULL);
	  }
	  else
	  {
	   StarCatalogSw.s = ILS_IDLE;
	   ICSetSwitch(&StarCatalogSw, "Catalog unavailable");
	  }
	  return;
	}

	if (!validateSwitch(s, &DeepSkyCatalogSw, NARRAY(DeepSkyCatalogS), index,1))
	{
	  if (index[0] == LX200_MESSIER_C)
	  {
	    currentCatalog = index[0];
	    DeepSkyCatalogSw.s = ILS_OK;
	    ICSetSwitch(&DeepSkyCatalogSw, NULL);
	    return;
	  }
	  else
	    currentCatalog = LX200_DEEPSKY_C;

	  if (selectSubCatalog(currentCatalog, index[0]))
	  {
	   currentSubCatalog = index[0];
	   DeepSkyCatalogSw.s = ILS_OK;
	   ICSetSwitch(&DeepSkyCatalogSw, NULL);
	  }
	  else
	  {
	   DeepSkyCatalogSw.s = ILS_IDLE;
	   ICSetSwitch(&DeepSkyCatalogSw, "Catalog unavailable");
	  }
	  return;
	}

	if (!validateSwitch(s, &SolarSw, NARRAY(SolarS), index, 1))
	{
          selectSubCatalog ( LX200_STAR_C, LX200_STAR);
	  selectCatalogObject( LX200_STAR_C, index[0] + 901);

	  //formatHMS ( (targetRA = getObjectRA()), RA.nstr);
	  formatSex( (targetRA = getObjectRA()), RA.nstr, XXYYZZ);
          //formatDMS ( (targetDEC = getObjectDEC()), DEC.nstr);
	  formatSex ( (targetDEC = getObjectDEC()), DEC.nstr, SXXYYZZ);

	  ObjectNo.s = ILS_OK;
	  SolarSw.s  = ILS_OK;

	  //strcpy(ObjectNo.nstr , n->nstr);
	  getObjectInfo(ObjectInfo.text);
	  ICSetNumber(&ObjectNo , "Object updated");
	  ICSetNumber(&RA, NULL);
	  ICSetNumber(&DEC, NULL);
	  ICSetText  (&ObjectInfo, NULL);
	  ICSetSwitch(&SolarSw, NULL);

	  if (currentCatalog == LX200_STAR_C || currentCatalog == LX200_DEEPSKY_C)
	  	selectSubCatalog( currentCatalog, currentSubCatalog);
	  return;
	}

	if (!validateSwitch(s, &TrackModeSw, NARRAY(TrackModeS), index, 1))
	{
	  trackingMode = index[0];
	  selectTrackingMode(trackingMode);
	  TrackModeSw.s = ILS_OK;
	  ICSetSwitch(&TrackModeSw, NULL);
	  return;
	}

	/*if (!validateSwitch(s, &TimeFormatSw, NARRAY(TimeFormatS), index, 1))
	{
	  fprintf(stderr, "in Time FORMAT\n");
          // we have 24 : AM : PM
	  // we don't toggle format if user changes from AM to PM or vice vesra (both are 12 hour format)
	  // we only change when there is a transition from 12 --> 24 or 24 ---> 12
	  if (timeFormat != LX200_24)
	  {

	    if (index[0] == LX200_24)
	    {
	      fprintf(stderr, "CURRENT time format is 24\n");
	      toggleTimeFormat();
	    }

	    fprintf(stderr, "Format did not change\n");
	  }
	  else
	  {
	      toggleTimeFormat();
	      fprintf(stderr, "LAST Time format is 24, toggling to 12\n");
	  }


	  timeFormat = index[0];

	  TimeFormatSw.s = ILS_OK;

	if (timeFormat == LX200_24)
	 ICSetSwitch(&TimeFormatSw, "Time format changed to 24 hours. Telescope time is not updated until a new time is set");
	else if (timeFormat == LX200_AM)
	    ICSetSwitch(&TimeFormatSw, "Time format changed to AM.  Telescope time is not updated until a new time is set");
	else
	  ICSetSwitch(&TimeFormatSw, "Time format changed to PM.  Telescope time is not updated until a new time is set");

	  return;
	}*/
}

void alterSwitches(int OnOff, ISwitches * s, int switchArraySize)
{
  // 0 Off
  // 1 On

  int i;
  ISState switchSt = OnOff ? ISS_ON : ISS_OFF;

  for (i = 0; i < switchArraySize; i++)
    s->sw[i].s = switchSt;

}

void LX200Generic::ISPoll()
{

	double dx, dy;
	double currentRA, currentDEC;
	
	switch (OnCoordSetSw.s)
	{
	case ILS_IDLE:
	     break;

	case ILS_BUSY:

	    fprintf(stderr , "Getting LX200 RA, DEC...\n");
	    currentRA = getLX200RA();
	    currentDEC = getLX200DEC();
	    dx = targetRA - currentRA;
	    dy = targetDEC - currentDEC;

	    //formatHMS(currentRA, RA.nstr);
	    formatSex ( currentRA, RA.nstr, XXYYZZ);
	    //formatDMS(currentDEC, DEC.nstr);
	    formatSex (currentDEC, DEC.nstr, SXXYYZZ);

	    if (dx < 0) dx *= -1;
	    if (dy < 0) dy *= -1;
	    fprintf(stderr, "targetRA is %f, currentRA is %f\n", (float) targetRA, (float) currentRA);
	    fprintf(stderr, "targetDEC is %f, currentDEC is %f\n", (float) targetDEC, (float) currentDEC);


	    //if (dx <= 0.5 && dy <= 0.5)

	    // FIXME I should probably use getNumOfBars. Try to figure out why it crashes
	    if (dx <= 0.001 && dy <= 0.001)
	    {
	        //SlewS[0].s = ISS_OFF;
		if (lastSet == 0)
			abortSlew();

		OnCoordSetSw.s = ILS_OK;
		currentRA = oldLX200RA = targetRA;
		currentDEC = oldLX200DEC = targetDEC;

		//formatHMS(targetRA, RA.nstr);
		//formatDMS(targetDEC, DEC.nstr);
		formatSex (targetRA, RA.nstr, XXYYZZ);
		formatSex (targetDEC, DEC.nstr, SXXYYZZ);

		ICSetNumber (&RA, NULL);
		ICSetNumber (&DEC, NULL);
		if (lastSet == 0)
			ICSetSwitch (&OnCoordSetSw, "Slew is complete");
		else
			ICSetSwitch (&OnCoordSetSw, "Slew is complete. Tracking...");

	    } else
	    {
		//ICMessage (mydev, "Slew in progress...");
		ICSetNumber (&RA, NULL);
		ICSetNumber (&DEC, NULL);
	    }
	    break;

	case ILS_OK:
	    break;

	case ILS_ALERT:
	    break;
	}

	switch (MovementSw.s)
	{
	  case ILS_IDLE:
	   break;
	 case ILS_BUSY:
	     currentRA = getLX200RA();
	     currentDEC = getLX200DEC();
	     //formatHMS(currentRA, RA.nstr);
	     //formatDMS(currentDEC, DEC.nstr);
	     formatSex(currentRA, RA.nstr, XXYYZZ);
	     formatSex(currentDEC, DEC.nstr, SXXYYZZ);
	     ICSetNumber (&RA, NULL);
	     ICSetNumber (&DEC, NULL);
	     break;
	 case ILS_OK:
	   break;
	 case ILS_ALERT:
	   break;
	 }

}

int LX200Generic::checkPower()
{

  if (PowerSw.s != ILS_OK)
  {
    ICMessage (mydev, "Cannot change a property while telescope is Off");
    return -1;
  }

  return 0;

}

int LX200Generic::validateSwitch(ISwitches *clientSw, ISwitches *driverSw, int driverArraySize, int index[], int validatePower)
{
//fprintf(stderr , "IN validate switch\n");
  int i, j;

  if (!strcmp (clientSw->name, driverSw->name))
  {
	  // check if the telescope is connected //
	 if (validatePower && (checkPower()))
	 {
	     driverSw->s = ILS_IDLE;
	     ICSetSwitch (driverSw, NULL);
	     return -1;
	 }

	 for (i = 0, j =0 ; i < driverArraySize; i++)
	 {
		if (!strcmp (clientSw->sw[0].name, driverSw->sw[i].name))
		{
		    index[j++] = i;
		    driverSw->sw[i].s = ISS_ON;
		} else
		    driverSw->sw[i].s = ISS_OFF;
	  }
	   return 0;
  }

    return -1;
 }



void LX200Generic::getBasicData()
{

  int dd, mm;

  checkLX200Format();
  getAlignment();

  timeFormat = getTimeFormat() == 24 ? LX200_24 : LX200_AM;

  //if (timeFormat == LX200_24)
  //{
//    TimeFormatSw.s   = ILS_IDLE;
    //TimeFormatS[0].s = ISS_ON;
    //ICSetSwitch(&TimeFormatSw, NULL);
  //}

  // We always do 24 hours
  if (timeFormat != LX200_24)
   toggleTimeFormat();

  //formatHMS (getLX200RA(), RA.nstr);
  formatSex ( (targetRA = getLX200RA()), RA.nstr, XXYYZZ);
  formatSex ( (targetDEC = getLX200DEC()), DEC.nstr, SXXYYZZ);
  getObjectInfo(ObjectInfo.text);

  ICSetNumber(&RA, NULL);
  ICSetNumber(&DEC, NULL);
  ICSetText  (&ObjectInfo, NULL);

  formatSex ( getLocalTime24(), lastTime, XXYYZZ);
  getCalenderDate(lastDate);
  formatDateTime(Time.text, lastTime, lastDate);

  formatSex (getSDTime(), SDTime.nstr, XXYYZZ);

  sprintf(UTCOffset.nstr, "%d", getUCTOffset());

  ICSetText(&Time, NULL);
  ICSetNumber(&UTCOffset, NULL);
  ICSetNumber(&SDTime, NULL);


  selectSite(currentSiteNum);
  getSiteLongitude(&dd, &mm);
  sprintf (SiteLong.nstr, "%03d:%02d", dd, mm);
  getSiteLatitude(&dd, &mm);
  sprintf (SiteLat.nstr, "%+03d:%02d", dd, mm);

  getSiteName( SiteName.text, currentSiteNum);

  sprintf(TrackingFreq.nstr, "%04.1f", (float) getTrackFreq());

  ICSetNumber (&SiteLong, NULL);
  ICSetNumber (&SiteLat,  NULL);
  ICSetText   (&SiteName, NULL);
  ICSetSwitch (&SitesSw, NULL);
  ICSetNumber (&TrackingFreq, NULL);

}

void LX200Generic::powerTelescope(ISwitches* s)
{
   fprintf(stderr , "In POWER\n");
    for (uint i= 0; i < NARRAY(PowerS); i++)
    {
        if (!strcmp (s->sw[0].name, PowerS[i].name))
	{

	    if (Connect(SerialUSBPort[portIndex]) < 0)
	    {
	        ICSetSwitch (&PowerSw, "Error connecting to Telescope. Telescope is Off");
	        return;
	    }

	    PowerS[i].s = ISS_ON;
	}


        else
		PowerS[i].s = ISS_OFF;

     }

     if (PowerSw.sw[0].s == ISS_ON && !testTelescope())
     {
        fprintf(stderr , "telescope test successffully\n");
	PowerSw.s = ILS_OK;
	ICSetSwitch (&PowerSw, "Telescope is On. Retrieving basic data...");
	getBasicData();
     }
     else
     {
	PowerSw.s = ILS_IDLE;

	if (PowerSw.sw[0].s == ISS_ON)
	{
   	  PowerS[0].s = ISS_OFF;
	  PowerS[1].s = ISS_ON;

	  ICSetSwitch (&PowerSw, "Telescope is not connected to the serial port");
	  fprintf(stderr , "telescope test failed\n");
	  Disconnect();
	}
        else
	{
	 ICSetSwitch (&PowerSw, "Telescope is off");
	 Disconnect();
	 cleanUP();
	}
     }
}


// LX200 implementation //

void LX200Generic::cleanUP()
{
 //ConnectSw.s = ILS_IDLE;
 //ICSetSwitch (&ConnectSw , NULL);
}

void LX200Generic::slewError(int slewCode)
{
    OnCoordSetSw.s = ILS_IDLE;

    if (slewCode == 1)
	ICSetSwitch (&OnCoordSetSw, "Object below horizon");
    else
	ICSetSwitch (&OnCoordSetSw, "Object below higher");

}

void LX200Generic::getAlignment()
{

   if (checkPower())
    return;

   char align = ACK();
   if (align < 0)
   {
     ICSetSwitch (&AlignmentSw, "Retrieving alignment failed.");
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

    AlignmentSw.s = ILS_OK;
    ICSetSwitch (&AlignmentSw, NULL);
    fprintf(stderr , "ACK success %c\n", align);
}


