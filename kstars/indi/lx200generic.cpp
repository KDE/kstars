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

#include "lx200driver.h"
#include "lx200GPS.h"

LX200Generic *telescope = NULL;
int MaxReticleFlashRate = 3;
char mydev[] = "LX200 Classic";

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

static ISwitch PowerS[]          = {{"Power On" , ISS_OFF},{"Power Off", ISS_ON}};
static ISwitch AlignmentS []     = {{"Polar", ISS_ON}, {"AltAz", ISS_OFF}, {"Land", ISS_OFF}};
static ISwitch SitesS[]          = {{"Site 1", ISS_ON}, {"Site 2", ISS_OFF},  {"Site 3", ISS_OFF},  {"Site 4", ISS_OFF}};
static ISwitch SlewModeS[]       = {{"Max", ISS_ON}, {"Find", ISS_OFF}, {"Centering", ISS_OFF}, {"Guide", ISS_OFF}};
static ISwitch OnCoordSetS[]     = {{"Idle", ISS_ON}, { "Slew", ISS_OFF }, { "Sync", ISS_OFF }};
static ISwitch TrackModeS[]      = {{ "Default", ISS_ON} , { "Lunar", ISS_OFF}, {"Manual", ISS_OFF}};
static ISwitch abortSlewS[]      = {{"Abort Slew/Track", ISS_OFF }};

static ISwitch MovementS[]       = {{"North", ISS_OFF}, {"West", ISS_OFF}, {"East", ISS_OFF}, {"South", ISS_OFF}};
static ISwitch haltMoveS[]       = {{"Northward", ISS_OFF}, {"Westward", ISS_OFF}, {"Eastward", ISS_OFF}, {"Southward", ISS_OFF}};

static ISwitch StarCatalogS[]    = {{"STAR", ISS_ON}, {"SAO", ISS_OFF}, {"GCVS", ISS_OFF}};
static ISwitch DeepSkyCatalogS[] = {{"NGC", ISS_ON}, {"IC", ISS_OFF}, {"UGC", ISS_OFF}, {"Caldwell", ISS_OFF}, {"Arp", ISS_OFF}, {"Abell", ISS_OFF}, {"Messier", ISS_OFF}};
static ISwitch SolarS[]          = { {"Select item...", ISS_ON}, {"Mercury", ISS_OFF}, {"Venus", ISS_OFF}, {"Moon", ISS_OFF}, {"Mars", ISS_OFF}, {"Jupiter", ISS_OFF}, {"Saturn", ISS_OFF},
				    {"Uranus", ISS_OFF}, {"Neptune", ISS_OFF}, {"Pluto", ISS_OFF}};

/* Fundamental group */
static ISwitches PowerSw	 = { mydev, "POWER" , PowerS, NARRAY(PowerS), ILS_IDLE, 0, COMM_GROUP };
//static ISwitches PortSw          = { mydev, "Ports" , PortS, NARRAY(PortS), ILS_IDLE, 0, COMM_GROUP};
static IText Port		 = { mydev, "Ports", NULL, ILS_IDLE, 0, COMM_GROUP};

/* Basic data group */
static ISwitches AlignmentSw     = { mydev, "Alignment", AlignmentS, NARRAY(AlignmentS), ILS_IDLE, 0, BASIC_GROUP };
static INumber RA          = { mydev, "RA", NULL, ILS_IDLE, 0 , BASIC_GROUP};
static INumber DEC         = { mydev, "DEC", NULL, ILS_IDLE, 0 , BASIC_GROUP};
static INumber minAlt      = { mydev, "minAlt", NULL, ILS_IDLE, 0, BASIC_GROUP};
static INumber maxAlt      = { mydev, "maxAlt", NULL, ILS_IDLE, 0, BASIC_GROUP};
static IText   ObjectInfo        = { mydev, "Object Info", NULL, ILS_IDLE, 0 , BASIC_GROUP};

/* Movement group */
static ISwitches OnCoordSetSw    = { mydev, "ONCOORDSET", OnCoordSetS, NARRAY(OnCoordSetS), ILS_IDLE, 0, BASIC_GROUP};
static ISwitches SlewModeSw      = { mydev, "Slew rate", SlewModeS, NARRAY(SlewModeS), ILS_IDLE, 0, MOVE_GROUP};
static ISwitches TrackModeSw     = { mydev, "Tracking Mode", TrackModeS, NARRAY(TrackModeS), ILS_IDLE, 0, MOVE_GROUP};
static INumber TrackingFreq      = { mydev, "Tracking Frequency", NULL, ILS_IDLE, 0, MOVE_GROUP};
static ISwitches abortSlewSw     = { mydev, "ABORTSLEW", abortSlewS, NARRAY(abortSlewS), ILS_IDLE, 0, MOVE_GROUP};
static ISwitches MovementSw      = { mydev, "Move toward", MovementS, NARRAY(MovementS), ILS_IDLE, 0, MOVE_GROUP};
static ISwitches haltMoveSw      = { mydev, "Halt movement", haltMoveS, NARRAY(haltMoveS), ILS_IDLE, 0, MOVE_GROUP};

/* Data & Time */
static IText Time                = { mydev, "TIME", NULL, ILS_IDLE, 0 , DATETIME_GROUP};
static INumber SDTime            = { mydev, "Sidereal Time", NULL, ILS_IDLE, 0, DATETIME_GROUP};
//static INumber UTCOffset         = { mydev, "UTC", NULL, ILS_IDLE, 0, DATETIME_GROUP};

/* Site managment */
static ISwitches SitesSw         = { mydev, "Sites", SitesS, NARRAY(SitesS), ILS_IDLE, 0, SITE_GROUP};
static IText   SiteName          = { mydev, "Site Name", NULL, ILS_IDLE, 0, SITE_GROUP};
static INumber SiteLong	         = { mydev, "LONG", NULL, ILS_IDLE, 0, SITE_GROUP};
static INumber SiteLat	         = { mydev, "LAT", NULL, ILS_IDLE, 0, SITE_GROUP};

/* Library group */
static ISwitches StarCatalogSw   = { mydev, "Star Catalogs", StarCatalogS, NARRAY(StarCatalogS), ILS_IDLE, 0, LIBRARY_GROUP};
static ISwitches DeepSkyCatalogSw= { mydev, "Deep Sky Catalogs", DeepSkyCatalogS, NARRAY(DeepSkyCatalogS), ILS_IDLE, 0,  LIBRARY_GROUP};
static ISwitches SolarSw         = { mydev, "SOLARSYSTEM", SolarS, NARRAY(SolarS), ILS_IDLE, 0, LIBRARY_GROUP};
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
//   UTCOffset.nstr    = strcpy (new char[4]  , "00");
   Time.text         = strcpy (new char[32]  , "YYYY-MM-DDTHH:MM:SS");
   SDTime.nstr       = strcpy (new char[16]  , "00:00:00");
   SiteLong.nstr     = strcpy (new char[9]  , "00:00");
   SiteLat.nstr      = strcpy (new char[9]  , "00:00");
   minAlt.nstr       = strcpy (new char[9]  , "00");
   maxAlt.nstr       = strcpy (new char[9]  , "90");
   ObjectNo.nstr     = strcpy (new char[5]  , "0000");
   TrackingFreq.nstr = strcpy (new char[4]  , "00.0");
   Port.text         = strcpy (new char[32] , "/dev/ttyS0");
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
   lastMove[0] = lastMove[1] = lastMove[2] = lastMove[3] = 0;

   // Children call parent routines, this is the default
   fprintf(stderr , "initilizaing from generic LX200 device...\n");

}

void LX200Generic::ISGetProperties(const char *dev)
{

 if (dev && strcmp (mydev, dev))
    return;

  ICDefSwitches (&PowerSw, "Power", ISP_W, IR_1OFMANY);
  ICDefText     (&Port , "Port", IP_RW);

  ICDefSwitches (&AlignmentSw, "Alignment", ISP_W, IR_1OFMANY);
  ICDefNumber (&minAlt, "Min Elevation Limit", IP_RW, NULL);
  ICDefNumber (&maxAlt, "Max Elevation Limit", IP_RW, NULL);
  ICDefNumber (&RA, "RA H:M:S", IP_RW, NULL);
  ICDefNumber (&DEC, "DEC D:M:S", IP_RW, NULL);
  ICDefSwitches (&OnCoordSetSw, "On Set", ISP_W, IR_1OFMANY);
  ICDefText   (&ObjectInfo, "Object Info", IP_RO);

  ICDefSwitches (&SlewModeSw, "Slew rate", ISP_W, IR_1OFMANY);
  ICDefSwitches (&TrackModeSw, "Tracking Mode", ISP_W, IR_1OFMANY);
  ICDefNumber   (&TrackingFreq, "Tracking Freq.", IP_RW, &FreqRange);
  ICDefSwitches (&abortSlewSw, "Abort", ISP_W, IR_1OFMANY);
  ICDefSwitches (&MovementSw, "Move toward", ISP_W, IR_1OFMANY);
  ICDefSwitches (&haltMoveSw, "Halt movement", ISP_W, IR_1OFMANY);

  ICDefText(&Time, "UTC Y-M-DTH:M:S", IP_RW);
  //ICDefNumber(&UTCOffset, "UTC Offset", IP_RW, NULL);
  ICDefNumber(&SDTime, "Sidereal Time H:M:S", IP_RW, NULL);

  ICDefSwitches (&SitesSw, "Select site", ISP_W, IR_1OFMANY);
  ICDefText     (&SiteName, "Site name", IP_RW);
  ICDefNumber   (&SiteLong, "Site Longitude D:M:S +E", IP_RW, NULL);
  ICDefNumber   (&SiteLat,  "Site Latitude  D:M:S +N", IP_RW, NULL);

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
	int UTCOffset;
	char localTime[64];
  	char localDate[64];
	struct tm *ltp = new tm;
	time_t ltime;
	time (&ltime);
	localtime_r (&ltime, ltp);


	// ignore if not ours //
	if (strcmp (t->dev, mydev))
	    return;

	if (!strcmp(t->name, Port.name) )
	{
	  Port.s = ILS_OK;
	  strcpy(Port.text, t->text);
	  ICSetText (&Port, NULL);
	  return;
	}

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


			UTCOffset = (h - ltp->tm_hour);

			extractDate(lastDate, &dd, &mm, &yy);
			if (dd - ltp->tm_mday != 0)
			 UTCOffset += 24;

			fprintf(stderr, "UTCOffset is %d\n", UTCOffset);
			fprintf(stderr, "time is %02d:%02d:%02d\n", ltp->tm_hour, m, s);
			setUTCOffset(UTCOffset);
	  		setLocalTime(ltp->tm_hour, m, s);
		  	Time.s = ILS_OK;

			strcpy(Time.text, t->text);
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

			// To make YYYY to YY
			//sprintf(lastDate, "%d:%d:%d", tp->tm_year, tp->tm_mon, tp->tm_mday);
			//fprintf(stderr, "The year is %d , month %d, day of month is %d\n",tp->tm_year, tp->tm_mon, tp->tm_mday);
			strftime (lastDate, sizeof(lastDate), "%Y:%m:%d", ltp);
			extractDate(lastDate, &dd, &mm, &yy);

			//fprintf(stderr, "date writtin is %d:%d:%d\n", dd, mm, yy);

			setCalenderDate(dd, mm, yy);
 			strcpy(Time.text, t->text);
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

    //idle
    case 0:
    if (RA.s == ILS_BUSY)
    {
      RA.s = ILS_OK;
      ICSetNumber(&RA, "RA coordinates stored.");
    }
    else if (DEC.s == ILS_BUSY)
    {
      DEC.s = ILS_OK;
      ICSetNumber(&DEC, "Dec coordinates stored.");
    }
    return (0);
    break;

    // Slew
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
	  //strcpy(RA.nstr, RAbuffer);
	  //strcpy(DEC.nstr, Decbuffer);
	  OnCoordSetSw.s = ILS_BUSY;
	  //ICSetNumber(&RA, NULL);
	  //ICSetNumber(&DEC, NULL);
	  ICSetSwitch(&OnCoordSetSw, "Slewing to RA %s - DEC %s", RAbuffer, Decbuffer);
	  break;

   // Track
   case 2:

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
  case 3:

	  OnCoordSetSw.s = ILS_OK;
	  Sync(syncString);

          formatSex(targetRA, RAbuffer, XXYYZZ);
	  formatSex(targetDEC, Decbuffer, SXXYYZZ);
	  strcpy(RA.nstr, RAbuffer);
	  strcpy(DEC.nstr, Decbuffer);
          RA.s = ILS_OK;
	  DEC.s = ILS_OK;
	  ICSetNumber(&RA, NULL);
	  ICSetNumber(&DEC, NULL);
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
	       RA.s = ILS_BUSY;
	       getSex(n->nstr, &targetRA);
	       getObjectInfo(ObjectInfo.text);
	       ICSetText   (&ObjectInfo, NULL);
	       //strcpy(RA.nstr, n->nstr);
	       //ICSetNumber(&RA, NULL);
	       if (handleCoordSet())
	       {
	        strcpy(RA.nstr, n->nstr);
	        RA.s = ILS_IDLE;
	    	ICSetNumber(&RA, NULL);
	       }

	       return;

	    }
	    else
	    {
	        strcpy(RA.nstr, n->nstr);
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
	       DEC.s = ILS_BUSY;
	       getSex(n->nstr, &targetDEC);
	       getObjectInfo(ObjectInfo.text);
	       ICSetText   (&ObjectInfo, NULL);
	       //strcpy(DEC.nstr, n->nstr);
	       //ICSetNumber(&DEC, NULL);
	       if (handleCoordSet())
	       {
	        strcpy(DEC.nstr, n->nstr);
	        DEC.s = ILS_IDLE;
	        ICSetNumber(&DEC, NULL);
	       }

	       return;

	    }
	    else
	    {
	        strcpy(DEC.nstr, n->nstr);
	        DEC.s = ILS_ALERT;
		ICSetNumber (&DEC, "Error setting DEC");
		return;
            }
	} // end DEC set

	/*if ( !strcmp (n->name, UTCOffset.name))
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
	} // end UTC Offset*/

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

	  sprintf(SiteLong.nstr, "%f:%f:%f", h, m, s);
	  double finalLong;
	  getSex(SiteLong.nstr, &finalLong);

	  formatSex( 360.0 - finalLong, SiteLong.nstr, XXXYY);
	  validateSex(SiteLong.nstr, &h, &m, &s);

	  setSiteLongitude((int) h, (int) m);
	  SiteLong.s = ILS_OK;
	  strcpy(SiteLong.nstr , n->nstr);
	  ICSetNumber(&SiteLong , "Site longitude updated to %s", n->nstr);
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
	  if (s > 60 || s < 0 || m > 59 || m < 0 || d < -90 || d > 90)
	  {
 	    SiteLong.s = ILS_IDLE;
	    ICSetNumber(&SiteLong , "Coordinates invalid");
	    return;
	  }


          setSiteLatitude( (int) d, (int) m, (int) s);
	  SiteLat.s = ILS_OK;
	  strcpy(SiteLat.nstr , n->nstr);
	  ICSetNumber(&SiteLat , "Site latitude updated to %s", n->nstr);
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
	  getObjectInfo(ObjectInfo.text);
	  ObjectNo.s = RA.s = DEC.s = ILS_OK;
	  ICSetNumber(&ObjectNo , "Object updated");
	  strcpy(ObjectNo.nstr , n->nstr);
	  ICSetNumber(&RA, NULL);
	  ICSetNumber(&DEC, NULL);
	  ICSetText  (&ObjectInfo, NULL);

	  handleCoordSet();

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

	if ( !strcmp (n->name, minAlt.name) )
	{

	 if (checkPower())
	  {
	    minAlt.s = ILS_IDLE;
	    ICSetNumber(&minAlt, NULL);
	    return;
	  }

	  sscanf(n->nstr, "%d", &num);

	  if (num > 90 || num < 0)
	  {
 	    minAlt.s = ILS_IDLE;
	    ICSetNumber(&minAlt , "Limit invalid (0 to 90 degrees)");
	    return;
	  }

	  fprintf(stderr, "Trying to set minimum elevation limit to %d", num);

	  if (setMinElevationLimit(num))
	  {
	    minAlt.s = ILS_IDLE;
	    ICSetNumber(&minAlt, "Error setting minimum elevation limit");
	    return;
	  }

	  strcpy(minAlt.nstr, n->nstr);
	  minAlt.s = ILS_OK;
	  ICSetNumber(&minAlt, NULL);
	}


	if ( !strcmp (n->name, maxAlt.name) )
	{

	 if (checkPower())
	  {
	    maxAlt.s = ILS_IDLE;
	    ICSetNumber(&maxAlt, NULL);
	    return;
	  }

	  sscanf(n->nstr, "%d", &num);

	  if (num > 90 || num < 0)
	  {
 	    maxAlt.s = ILS_IDLE;
	    ICSetNumber(&maxAlt , "Limit invalid (0 to 90 degrees)");
	    return;
	  }

	  fprintf(stderr, "Trying to set maximum elevation limit to %d", num);

	  if (setMaxElevationLimit(num))
	  {
	    maxAlt.s = ILS_IDLE;
	    ICSetNumber(&maxAlt, "Error setting maximum elevation limit");
	    return;
	  }

	  strcpy(maxAlt.nstr, n->nstr);
	  maxAlt.s = ILS_OK;
	  ICSetNumber(&maxAlt, NULL);
	}
}

void LX200Generic::ISNewSwitch(ISwitches *s)
{

	int index[16];
	int dd, mm;
	//static int nOfMovements = 0;

	// ignore if not ours //
	if (strcmp (s->dev, mydev))
	    return;

	// FIRST Switch ALWAYS for power
	if (!strcmp (s->name, PowerSw.name))
	{
   	 powerTelescope(s);
	 return;
	}

	if (!validateSwitch(s, &OnCoordSetSw, NARRAY(OnCoordSetS), index, 0))
	{
	  lastSet = index[0];
	  //OnCoordSetSw.s = ILS_OK;
	  //ICSetSwitch(&OnCoordSetSw, NULL);
	  handleCoordSet();
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
          if (lastMove[index[0]])
	    return;

	  lastMove[index[0]] = 1;

	  MoveTo(index[0]);

	  for (uint i=0; i < 4; i++)
	    MovementSw.sw[i].s = lastMove[i] == 0 ? ISS_OFF : ISS_ON;

	  MovementSw.s = ILS_BUSY;
	  ICSetSwitch(&MovementSw, "Moving %s...", Direction[index[0]]);
	  return;
	}

	if (!validateSwitch(s, &haltMoveSw, NARRAY(haltMoveS), index, 1))
	{
	  if (MovementSw.s == ILS_BUSY)
	  {
	  	HaltMovement(index[0]);
		lastMove[index[0]] = 0;

		if (!lastMove[0] && !lastMove[1] && !lastMove[2] && !lastMove[3])
		  MovementSw.s = ILS_IDLE;

		for (uint i=0; i < 4; i++)
		{
	    	   haltMoveSw.sw[i].s = ISS_OFF;
		   MovementSw.sw[i].s = lastMove[i] == 0 ? ISS_OFF : ISS_ON;
		}

		RA.s = ILS_IDLE;
		DEC.s = ILS_IDLE;
                haltMoveSw.s = ILS_IDLE;

		ICSetSwitch(&haltMoveSw, "Moving toward %s aborted", Direction[index[0]]);
	  	ICSetSwitch(&MovementSw, NULL);
	  }
	  else
	  {
	        haltMoveSw.sw[index[0]].s = ISS_OFF;
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
	  // We ignore the first option : "Select item"
	  if (index[0] == 0)
	  {
	    SolarSw.s  = ILS_IDLE;
	    ICSetSwitch(&SolarSw, NULL);
	    return;
	  }

          selectSubCatalog ( LX200_STAR_C, LX200_STAR);
	  selectCatalogObject( LX200_STAR_C, index[0] + 900);

	  formatSex( (targetRA = getObjectRA()), RA.nstr, XXYYZZ);
          formatSex ( (targetDEC = getObjectDEC()), DEC.nstr, SXXYYZZ);

	  ObjectNo.s = ILS_OK;
	  SolarSw.s  = ILS_OK;
	  RA.s = DEC.s = ILS_IDLE;

	  //strcpy(ObjectNo.nstr , n->nstr);
	  getObjectInfo(ObjectInfo.text);
	  ICSetNumber(&ObjectNo , "Object updated");
	  ICSetNumber(&RA, NULL);
	  ICSetNumber(&DEC, NULL);
	  ICSetText  (&ObjectInfo, NULL);
	  ICSetSwitch(&SolarSw, NULL);

	  if (currentCatalog == LX200_STAR_C || currentCatalog == LX200_DEEPSKY_C)
	  	selectSubCatalog( currentCatalog, currentSubCatalog);

	  handleCoordSet();

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

	    formatSex ( currentRA, RA.nstr, XXYYZZ);
	    formatSex (currentDEC, DEC.nstr, SXXYYZZ);

	    if (dx < 0) dx *= -1;
	    if (dy < 0) dy *= -1;
	    fprintf(stderr, "targetRA is %f, currentRA is %f\n", (float) targetRA, (float) currentRA);
	    fprintf(stderr, "targetDEC is %f, currentDEC is %f\n****************************\n", (float) targetDEC, (float) currentDEC);


	    if (getNumberOfBars() == 0 || (dx <= 0.001 && dy <= 0.001))
	    {

		OnCoordSetSw.s = ILS_OK;
		currentRA = targetRA;
		currentDEC = targetDEC;

		formatSex (targetRA, RA.nstr, XXYYZZ);
		formatSex (targetDEC, DEC.nstr, SXXYYZZ);

		RA.s = ILS_OK;
		DEC.s = ILS_OK;
		ICSetNumber (&RA, NULL);
		ICSetNumber (&DEC, NULL);
		ICSetSwitch (&OnCoordSetSw, "Slew is complete");
	    } else
	    {
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
    ICMessage (mydev, "Cannot change a property while the telescope is offline");
    return -1;
  }

  return 0;

}

int LX200Generic::validateSwitch(ISwitches *clientSw, ISwitches *driverSw, int driverArraySize, int index[], int validatePower)
{
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

  // We always do 24 hours
  if (timeFormat != LX200_24)
   toggleTimeFormat();

  formatSex ( (targetRA = getLX200RA()), RA.nstr, XXYYZZ);
  formatSex ( (targetDEC = getLX200DEC()), DEC.nstr, SXXYYZZ);
  getObjectInfo(ObjectInfo.text);

  sprintf(minAlt.nstr, "%02d", getMinElevationLimit());
  sprintf(maxAlt.nstr, "%02d", getMaxElevationLimit());

  formatSex ( getLocalTime24(), lastTime, XXYYZZ);
  getCalenderDate(lastDate);
  formatDateTime(Time.text, lastTime, lastDate);
  formatSex (getSDTime(), SDTime.nstr, XXYYZZ);

  selectSite(currentSiteNum);
  getSiteLongitude(&dd, &mm);
  sprintf (SiteLong.nstr, "%03d:%02d", dd, mm);
  getSiteLatitude(&dd, &mm);
  sprintf (SiteLat.nstr, "%+03d:%02d", dd, mm);
  getSiteName( SiteName.text, currentSiteNum);
  sprintf(TrackingFreq.nstr, "%04.1f", (float) getTrackFreq());

  ICSetNumber (&RA, NULL);
  ICSetNumber (&DEC, NULL);
  ICSetNumber (&minAlt, NULL);
  ICSetNumber (&maxAlt, NULL);
  ICSetNumber (&SDTime, NULL);
  ICSetNumber (&SiteLong, NULL);
  ICSetNumber (&SiteLat,  NULL);
  ICSetNumber (&TrackingFreq, NULL);
  ICSetText   (&ObjectInfo, NULL);
  ICSetText   (&Time, NULL);
  ICSetText   (&SiteName, NULL);
  ICSetSwitch (&SitesSw, NULL);
}

void LX200Generic::powerTelescope(ISwitches* s)
{
   fprintf(stderr , "In POWER\n");
    for (uint i= 0; i < NARRAY(PowerS); i++)
    {
        if (!strcmp (s->sw[0].name, PowerS[i].name))
	{

	    if (Connect(Port.text) < 0)
	    {
	        ICSetSwitch (&PowerSw, "Error connecting to Telescope. Telescope is offline.");
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
	ICSetSwitch (&PowerSw, "Telescope is online. Retrieving basic data...");
	getBasicData();
     }
     else
     {
	PowerSw.s = ILS_IDLE;

	if (PowerSw.sw[0].s == ISS_ON)
	{
   	  PowerS[0].s = ISS_OFF;
	  PowerS[1].s = ISS_ON;

	  ICSetSwitch (&PowerSw, "Telescope is not connected to the serial/usb port");
	  fprintf(stderr , "telescope test failed\n");
	  Disconnect();
	}
        else
	{
	 ICSetSwitch (&PowerSw, "Telescope is offline.");
	 Disconnect();
	}
     }
}


void LX200Generic::slewError(int slewCode)
{
    OnCoordSetSw.s = ILS_IDLE;

    if (slewCode == 1)
	ICSetSwitch (&OnCoordSetSw, "Object below horizon");
    else
	ICSetSwitch (&OnCoordSetSw, "Object below the minimum elevation limit");

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


