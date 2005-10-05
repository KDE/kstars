#if 0
    Astro-Physics driver
    Copyright (C) 2005 Jasem Mutlaq (mutlaqja@ikarustech.com)

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
#include "apmount.h"

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

APMount *telescope = NULL;
int MaxReticleFlashRate = 3;
extern char* me;

/* There is _one_ binary for all LX200 drivers, but each binary is renamed
** to its device name (i.e. lx200gps, lx200_16..etc). The main function will
** fetch from std args the binary name and ISInit will create the apporpiate
** device afterwards. If the binary name does not match any known devices,
** we simply create a generic device
*/


#define COMM_GROUP	"Communication"
#define BASIC_GROUP	"Main Control"
#define MOVE_GROUP	"Movement Control"
#define FOCUS_GROUP     "Focus Control"

#define mydev		"Astro-Physics"
#define currentRA	EqN[0].value
#define currentDEC	EqN[1].value

#define RA_THRESHOLD	0.01
#define DEC_THRESHOLD	0.05
#define LX200_SLEW	0
#define LX200_TRACK	1
#define LX200_SYNC	2
#define LX200_PARK	3

static void ISPoll(void *);
static void retryConnection(void *);

/*INDI controls */


/* send client definitions of all properties */
void ISInit()
{
  static int isInit=0;

 if (isInit)
  return;

 isInit = 1;
  
  telescope = new APMount();
  IEAddTimer (POLLMS, ISPoll, NULL);
}

void ISGetProperties (const char *dev)
{
 ISInit(); 
 telescope->ISGetProperties(dev);
}

void ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
 ISInit();
 telescope->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
 ISInit();
 telescope->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
 ISInit();
 telescope->ISNewNumber(dev, name, values, names, n);
}

void ISPoll (void */*p*/)
{
 telescope->ISPoll(); 
 IEAddTimer (POLLMS, ISPoll, NULL);
}

void ISNewBLOB (const char */*dev*/, const char */*name*/, int */*sizes[]*/, char **/*blobs[]*/, char **/*formats[]*/, char **/*names[]*/, int /*n*/)
{}

/**************************************************
*** AP Mount
***************************************************/

APMount::APMount()
{
   struct tm *utp;
   time_t t;
   time (&t);
   utp = gmtime (&t);

   initProperties();

   currentSiteNum = 1;
   trackingMode   = 2;
   lastSet        = -1;
   fault          = false;
   simulation     = false;
   targetRA       = 0;
   targetDEC      = 0;
   currentSet     = 0;
   UTCOffset      = 0;
   lastMove[0] = lastMove[1] = lastMove[2] = lastMove[3] = 0;

   localTM = new tm;
   
   utp->tm_mon  += 1;
   utp->tm_year += 1900;
   JD = UTtoJD(utp);
   
   IDLog("Julian Day is %g\n", JD);
   IDLog("Initilizing from Astro-Physics device...\n");
   IDLog("Driver Version: 2005-05-26\n");
 
   //enableSimulation(true);  
}

void APMount::initProperties()
{

   fillSwitch(&AlignmentS[0], "Polar", "", ISS_ON);
   fillSwitch(&AlignmentS[1], "AltAz", "", ISS_OFF);
   fillSwitchVector(&AlignmentSP, AlignmentS, NARRAY(AlignmentS), mydev, "Alignment", "", COMM_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

  fillSwitch(&PowerS[0], "CONNECT", "Connect", ISS_OFF);
  fillSwitch(&PowerS[1], "DISCONNECT", "Disconnect", ISS_ON);
  fillSwitchVector(&PowerSP, PowerS, NARRAY(PowerS), mydev, "CONNECTION", "Connection", COMM_GROUP, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

  fillSwitch(&OnCoordSetS[0], "SLEW", "Slew", ISS_ON);
  fillSwitch(&OnCoordSetS[1], "TRACK", "Track", ISS_OFF);
  fillSwitch(&OnCoordSetS[2], "SYNC", "Sync", ISS_OFF);
  fillSwitchVector(&OnCoordSetSP, OnCoordSetS, NARRAY(OnCoordSetS), mydev, "ON_COORD_SET", "On Set", BASIC_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

  fillSwitch(&TrackModeS[0], "Lunar", "", ISS_OFF);
  fillSwitch(&TrackModeS[1], "Solar", "", ISS_OFF);
  fillSwitch(&TrackModeS[2], "Sideral", "", ISS_ON);
  fillSwitch(&TrackModeS[3], "Zero", "", ISS_OFF);
  fillSwitchVector(&TrackModeSP, TrackModeS, NARRAY(TrackModeS), mydev, "Tracking Mode", "",  MOVE_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

   fillSwitch(&AbortSlewS[0], "ABORT", "Abort", ISS_OFF);
   fillSwitchVector(&AbortSlewSP, AbortSlewS, NARRAY(AbortSlewS), mydev, "ABORT_MOTION", "Abort Slew/Track", BASIC_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    fillSwitch(&ParkS[0], "PARK", "Park", ISS_OFF);
    fillSwitch(&ParkS[1], "UNPARK", "Unpark", ISS_OFF);
    fillSwitchVector(&ParkSP, ParkS, NARRAY(ParkS), mydev, "PARK", "Park Scope", BASIC_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    fillSwitch(&MovementS[0], "N", "North", ISS_OFF); 
    fillSwitch(&MovementS[1], "W", "West", ISS_OFF); 
    fillSwitch(&MovementS[2], "E", "East", ISS_OFF); 
    fillSwitch(&MovementS[3], "S", "South", ISS_OFF); 
    fillSwitchVector(&MovementSP, MovementS, NARRAY(MovementS), mydev, "MOVEMENT", "Move toward", MOVE_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

  fillSwitch(&FocusMotionS[0], "IN", "Focus in", ISS_OFF);
  fillSwitch(&FocusMotionS[1], "OUT", "Focus out", ISS_OFF);
  fillSwitchVector(&FocusMotionSP, FocusMotionS, NARRAY(FocusMotionS), mydev, "FOCUS_MOTION", "Motion", FOCUS_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

  fillText(&PortT[0], "PORT", "Port", "/dev/ttyS0");
  fillTextVector(&PortTP, PortT, NARRAY(PortT), mydev, "DEVICE_PORT", "Ports", COMM_GROUP, IP_RW, 0, IPS_IDLE);

  fillText(&UTCT[0], "UTC", "", "YYYY-MM-DDTHH:MM:SS");
  fillTextVector(&TimeTP, UTCT, NARRAY(UTCT), mydev, "TIME", "UTC Time", COMM_GROUP, IP_RW, 0, IPS_IDLE);

  fillText(&ObjectT[0], "OBJECT_NAME", "Name", "--");
  fillTextVector(&ObjectTP, ObjectT, NARRAY(ObjectT), mydev, "OBJECT_INFO", "Object", BASIC_GROUP, IP_RW, 0, IPS_IDLE);

   fillNumber(&EqN[0], "RA", "RA  H:M:S", "%10.6m",  0., 24., 0., 0.);
   fillNumber(&EqN[1], "DEC", "Dec D:M:S", "%10.6m", -90., 90., 0., 0.);
   fillNumberVector(&EqNP, EqN, NARRAY(EqN), mydev, "EQUATORIAL_EOD_COORD" , "Equatorial JNow", BASIC_GROUP, IP_RW, 0, IPS_IDLE);

   fillNumber(&GeoN[0], "LAT",  "Lat.  D:M:S +N", "%10.6m",  -90.,  90., 0., 0.);
   fillNumber(&GeoN[1], "LONG", "Long. D:M:S +E", "%10.6m", 0., 360., 0., 0.);
   fillNumberVector(&GeoNP, GeoN, NARRAY(GeoN), mydev, "GEOGRAPHIC_COORD", "Geographic Location", COMM_GROUP, IP_RW, 0, IPS_IDLE);

   fillNumber(&FocusTimerN[0], "TIMEOUT", "Timeout (s)", "%10.6m", 0., 120., 1., 0.);
   fillNumberVector(&FocusTimerNP, FocusTimerN, NARRAY(FocusTimerN), mydev, "FOCUS_TIMEOUT", "Focus Timer", FOCUS_GROUP, IP_RW, 0, IPS_IDLE);

   fillNumber(&SDTimeN[0], "LST", "Sidereal time", "%10.6m" , 0.,24.,0.,0.);
   fillNumberVector(&SDTimeNP, SDTimeN, NARRAY(SDTimeN), mydev, "SDTIME", "Sidereal Time", COMM_GROUP, IP_RW, 0, IPS_IDLE);

   fillNumber(&HorN[0], "ALT",  "Alt  D:M:S", "%10.6m",  -90., 90., 0., 0.);
   fillNumber(&HorN[1], "AZ", "Az D:M:S", "%10.6m", 0., 360., 0., 0.);
   fillNumberVector(&HorNP, HorN, NARRAY(HorN), mydev, "HORIZONTAL_COORD", "Horizontal Coords", BASIC_GROUP, IP_RW, 0, IPS_IDLE);

   fillNumber(&FocusSpeedN[0], "SPEED", "Speed", "%0.f", 0., 3., 1., 0.);
   fillNumberVector(&FocusSpeedNP, FocusSpeedN, NARRAY(FocusSpeedN), mydev, "FOCUS_SPEED", "Speed", FOCUS_GROUP, IP_RW, 0, IPS_IDLE);


}

void APMount::ISGetProperties(const char *dev)
{

 if (dev && strcmp (mydev, dev))
    return;

  // COMM_GROUP
  IDDefSwitch(&PowerSP, NULL);
  IDDefText(&PortTP, NULL);
  IDDefSwitch(&AlignmentSP, NULL);
  IDDefText(&TimeTP, NULL);
  IDDefNumber(&SDTimeNP, NULL);
  IDDefNumber(&GeoNP, NULL);
  
  // MAIN CONTROL
  IDDefText(&ObjectTP, NULL);
  IDDefNumber(&EqNP, NULL);
  //IDDefNumber(&HorNP, NULL);
  IDDefSwitch(&OnCoordSetSP, NULL);
  IDDefSwitch(&AbortSlewSP, NULL);
  IDDefSwitch(&ParkSP, NULL);
  
  // MOVEMENT CONTROL
  IDDefSwitch(&TrackModeSP, NULL);
  IDDefSwitch(&MovementSP, NULL);
  
  // FOCUS CONTROL
  IDDefNumber(&FocusSpeedNP, NULL);
  IDDefSwitch(&FocusMotionSP, NULL);
  IDDefNumber(&FocusTimerNP, NULL);

  /* Send the basic data to the new client if the previous client(s) are already connected. */		
   if (PowerSP.s == IPS_OK)
       getBasicData();

}

void APMount::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int /*n*/)
{
	int err;
	struct tm *ltp = new tm;
	struct tm utm;
	time_t ltime;
	time (&ltime);
	localtime_r (&ltime, ltp);
	IText *tp;

	// ignore if not ours 
	if (strcmp (dev, mydev))
	    return;

	// Port name
	if (!strcmp(name, PortTP.name) )
	{
	  PortTP.s = IPS_OK;
	  tp = IUFindText( &PortTP, names[0] );
	  if (!tp)
	   return;

	   IUSaveText(tp, texts[0]);
 	   IDSetText (&PortTP, NULL);
	  return;
	}

       // Time
       if (!strcmp (name, TimeTP.name))
       {
	  if (checkPower(&TimeTP))
	   return;
	  
	  if (extractISOTime(texts[0], &utm) < 0)
	  {
	    TimeTP.s = IPS_IDLE;
	    IDSetText(&TimeTP , "Time invalid");
	    return;
	  }
		ltp->tm_mon  += 1;
		ltp->tm_year += 1900;

		tzset();
		
		UTCOffset = timezoneOffset();
		
		IDLog("local time is %02d:%02d:%02d\nUTCOffset: %g\n", ltp->tm_hour, ltp->tm_min, ltp->tm_sec, UTCOffset);
		
		getSDTime(&SDTimeN[0].value);
		IDSetNumber(&SDTimeNP, NULL);
		
		if ( ( err = setUTCOffset(UTCOffset) < 0) )
	  	{
	        TimeTP.s = IPS_ALERT;
	        IDSetText( &TimeTP , "Setting UTC Offset failed.");
		return;
	  	}
		
		if ( ( err = setLocalTime(ltp->tm_hour, ltp->tm_min, ltp->tm_sec) < 0) )
	  	{
	          handleError(&TimeTP, err, "Setting local time");
        	  return;
	  	}

		tp = IUFindText(&TimeTP, names[0]);
		if (!tp)
		     return;
                 IUSaveText(tp, texts[0]);
		TimeTP.s = IPS_OK;

		// update JD
                JD = UTtoJD(&utm);

		IDLog("New JD is %f\n", (float) JD);

		if ((localTM->tm_mday == ltp->tm_mday ) && (localTM->tm_mon == ltp->tm_mon) &&
		    (localTM->tm_year == ltp->tm_year))
		{
		  IDSetText(&TimeTP , "Time updated to %s", texts[0]);
		  return;
		}

	        delete (localTM);
		localTM = ltp;
		
		if ( ( err = setCalenderDate(ltp->tm_mday, ltp->tm_mon, ltp->tm_year) < 0) )
	  	{
		  		handleError(&TimeTP, err, "Setting local date.");
		  		return;
		}
		
 		IDSetText(&TimeTP , "Date changed, updating planetary data...");
	}

       if (!strcmp (name, ObjectTP.name))
       {
	  if (checkPower(&ObjectTP))
	   return;

          IUSaveText(&ObjectT[0], texts[0]);
          ObjectTP.s = IPS_OK;
          IDSetText(&ObjectTP, NULL);
          return;
       }
          
}


void APMount::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
	int h =0, m =0, s=0, err;
	double newRA =0, newDEC =0;
	
	// ignore if not ours //
	if (strcmp (dev, mydev))
	    return;

	if (!strcmp (name, EqNP.name))
	{
	  int i=0, nset=0;

	  if (checkPower(&EqNP))
	   return;

	    for (nset = i = 0; i < n; i++)
	    {
		INumber *eqp = IUFindNumber (&EqNP, names[i]);
		if (eqp == &EqN[0])
		{
                    newRA = values[i];
		    nset += newRA >= 0 && newRA <= 24.0;
		} else if (eqp == &EqN[1])
		{
		    newDEC = values[i];
		    nset += newDEC >= -90.0 && newDEC <= 90.0;
		}
	    }

	  if (nset == 2)
	  {
	   char RAStr[32], DecStr[32];

	   fs_sexa(RAStr, newRA, 2, 3600);
	   fs_sexa(DecStr, newDEC, 2, 3600);
	  
	   IDLog("We received JNow RA %g - DEC %g\n", newRA, newDEC);
	   IDLog("We received JNow RA %s - DEC %s\n", RAStr, DecStr);
	   
	   if ( (err = setObjectRA(newRA)) < 0 || ( err = setObjectDEC(newDEC)) < 0)
	   {
	     handleError(&EqNP, err, "Setting RA/DEC");
	     return;
	   } 
	   
	   targetRA  = newRA;
	   targetDEC = newDEC;
	   
	   if (MovementSP.s == IPS_BUSY)
	   {
	   	for (int i=0; i < 4; i++)
	   	{
	     		lastMove[i] = 0;
	     		MovementS[i].s = ISS_OFF;
	   	}
		
		MovementSP.s = IPS_IDLE;
		IDSetSwitch(&MovementSP, NULL);
	   }
	   
	   if (handleCoordSet())
	   {
	     EqNP.s = IPS_IDLE;
	     IDSetNumber(&EqNP, NULL);
	     
	   }
	} // end nset
	else
	{
		EqNP.s = IPS_IDLE;
		IDSetNumber(&EqNP, "RA or Dec missing or invalid");
	}

	    return;
     } /* end EqNP */

	// Sideral Time
        if ( !strcmp (name, SDTimeNP.name) )
	{

	  if (checkPower(&SDTimeNP))
	   return;

	  if (values[0] < 0.0 || values[0] > 24.0)
	  {
	    SDTimeNP.s = IPS_IDLE;
	    IDSetNumber(&SDTimeNP , "Time invalid");
	    return;
	  }

	  getSexComponents(values[0], &h, &m, &s);
	  IDLog("Time is %02d:%02d:%02d\n", h, m, s);
	  
	  if ( ( err = setSDTime(h, m, s) < 0) )
	  {
	    handleError(&SDTimeNP, err, "Setting siderial time"); 
            return;
	  }
	  
	  SDTimeNP.np[0].value = values[0];
	  SDTimeNP.s = IPS_OK;

	  IDSetNumber(&SDTimeNP , "Sidereal time updated to %02d:%02d:%02d", h, m, s);

	  return;
        }

	// Geographical location
	if (!strcmp (name, GeoNP.name))
	{
	    // new geographic coords
	    double newLong = 0, newLat = 0;
	    int i, nset;
	    char msg[128];

	  if (checkPower(&GeoNP))
	   return;

	    for (nset = i = 0; i < n; i++)
	    {
		INumber *geop = IUFindNumber (&GeoNP, names[i]);
		if (geop == &GeoN[0])
		{
		    newLat = values[i];
		    nset += newLat >= -90.0 && newLat <= 90.0;
		} else if (geop == &GeoN[1])
		{
		    newLong = values[i];
		    nset += newLong >= 0.0 && newLong < 360.0;
		}
	    }

	    if (nset == 2)
	    {
		char l[32], L[32];
		GeoNP.s = IPS_OK;
		fs_sexa (l, newLat, 3, 3600);
		fs_sexa (L, newLong, 4, 3600);
		
		if ( ( err = setSiteLongitude(360.0 - newLong) < 0) )
	        {
		   handleError(&GeoNP, err, "Setting site coordinates");
		   return;
	         }
		
		setSiteLatitude(newLat);
		GeoNP.np[0].value = newLat;
		GeoNP.np[1].value = newLong;
		snprintf (msg, sizeof(msg), "Site location updated to Lat %.32s - Long %.32s", l, L);
	    } else
	    {
		GeoNP.s = IPS_IDLE;
		strcpy(msg, "Lat or Long missing or invalid");
	    }
	    IDSetNumber (&GeoNP, "%s", msg);
	    return;
	}
	
        // Focus TImer
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
	    FocusMotionSP.s = IPS_IDLE;
	    FocusTimerNP.s  = IPS_IDLE;
	    IDSetSwitch(&FocusMotionSP, NULL);
	    IDSetNumber(&FocusTimerNP, NULL);
	  }
	    
	  setFocuserSpeedMode( ( (int) FocusSpeedN[0].value));
	  FocusSpeedNP.s = IPS_OK;
	  IDSetNumber(&FocusSpeedNP, NULL);
	  return;
	}

}

void APMount::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
	int index;
	int err;
	char combinedDir[64];
	ISwitch *swp;

	// ignore if not ours //
	if (strcmp (mydev, dev))
	    return;

	// Connection
	if (!strcmp (name, PowerSP.name))
	{
	 IUResetSwitches(&PowerSP);
	 IUUpdateSwitches(&PowerSP, states, names, n);
   	 powerTelescope();
	 return;
	}

	// Coord set
	if (!strcmp(name, OnCoordSetSP.name))
	{
  	  if (checkPower(&OnCoordSetSP))
	   return;

	  IUResetSwitches(&OnCoordSetSP);
	  IUUpdateSwitches(&OnCoordSetSP, states, names, n);
	  currentSet = getOnSwitch(&OnCoordSetSP);
	  OnCoordSetSP.s = IPS_OK;
	  IDSetSwitch(&OnCoordSetSP, NULL);
	}
	
	// Parking
	if (!strcmp(name, ParkSP.name))
	{
	  if (checkPower(&ParkSP))
	    return;
           
	   IUResetSwitches(&ParkSP);
	   IUUpdateSwitches(&ParkSP, states, names, n);
	   index = getOnSwitch(&ParkSP);

	   // Park Command
	   if (index == 0)
		APPark();
	   else
	   // Unpark
	   {
		char **tmtexts = (char **) malloc(1);
		char **tmtp    = (char **) malloc(1);
		tmtexts[0]     = (char *) malloc (32);
		tmtp[0]	       = (char *) malloc (32);

		strcpy(tmtexts[0], timestamp());
		strcpy(tmtp[0], "UTC");

		// Update date and time before unparking
		ISNewText(mydev, "TIME", tmtexts, tmtp, 1);
		APUnpark();

		free (tmtexts);
		free (tmtp);
	   }

	   ParkSP.s = IPS_OK;
	   IDSetSwitch(&ParkSP, (index == 0) ? "Parking..." : "Unparking...");
	   return;
	}
	  
	// Abort Slew
	if (!strcmp (name, AbortSlewSP.name))
	{
	  if (checkPower(&AbortSlewSP))
	  {
	    AbortSlewSP.s = IPS_IDLE;
	    IDSetSwitch(&AbortSlewSP, NULL);
	    return;
	  }
	  
	  IUResetSwitches(&AbortSlewSP);
	  abortSlew();

	    if (EqNP.s == IPS_BUSY)
	    {
		AbortSlewSP.s = IPS_OK;
		EqNP.s       = IPS_IDLE;
		IDSetSwitch(&AbortSlewSP, "Slew aborted.");
		IDSetNumber(&EqNP, NULL);
            }
	    else if (MovementSP.s == IPS_BUSY)
	    {
	        
		for (int i=0; i < 4; i++)
		  lastMove[i] = 0;
		
		MovementSP.s  = IPS_IDLE; 
		AbortSlewSP.s = IPS_OK;		
		EqNP.s       = IPS_IDLE;
		IUResetSwitches(&MovementSP);
		IUResetSwitches(&AbortSlewSP);
		IDSetSwitch(&AbortSlewSP, "Slew aborted.");
		IDSetSwitch(&MovementSP, NULL);
		IDSetNumber(&EqNP, NULL);
	    }
	    else
	    {
	        IUResetSwitches(&MovementSP);
	        AbortSlewSP.s = IPS_OK;
	        IDSetSwitch(&AbortSlewSP, NULL);
	    }

	    return;
	}

	// Alignment
	if (!strcmp (name, AlignmentSP.name))
	{
	  if (checkPower(&AlignmentSP))
	   return;

	  IUResetSwitches(&AlignmentSP);
	  IUUpdateSwitches(&AlignmentSP, states, names, n);
	  index = getOnSwitch(&AlignmentSP);

	  if ( ( err = setAlignmentMode(index) < 0) )
	  {
	     handleError(&AlignmentSP, err, "Setting alignment");
             return;
	  }
	  
	  AlignmentSP.s = IPS_OK;
          IDSetSwitch (&AlignmentSP, NULL);
	  return;

	}

        // Focus Motion
	if (!strcmp (name, FocusMotionSP.name))
	{
	  if (checkPower(&FocusMotionSP))
	   return;

	  IUResetSwitches(&FocusMotionSP);
	  
	  // If speed is "halt"
	  if (FocusSpeedN[0].value == 0)
	  {
	    FocusMotionSP.s = IPS_IDLE;
	    IDSetSwitch(&FocusMotionSP, NULL);
	    return;
	  }
	  
	  IUUpdateSwitches(&FocusMotionSP, states, names, n);
	  index = getOnSwitch(&FocusMotionSP);
	  
	  if ( ( err = setFocuserMotion(index) < 0) )
	  {
	     handleError(&FocusMotionSP, err, "Setting focuser speed");
             return;
	  }
	  
	  FocusMotionSP.s = IPS_BUSY;
	  
	  // with a timer 
	  if (FocusTimerN[0].value > 0)  
	     FocusTimerNP.s  = IPS_BUSY;
	  
	  IDSetSwitch(&FocusMotionSP, NULL);
	  return;
	}

	// Movement
	if (!strcmp (name, MovementSP.name))
	{
	  if (checkPower(&MovementSP))
	   return;

	 index = -1;
	 IUUpdateSwitches(&MovementSP, states, names, n);
	 swp = IUFindSwitch(&MovementSP, names[0]);
	 
	 if (!swp)
	 {
	    abortSlew();
	    IUResetSwitches(&MovementSP);
	    MovementSP.s = IPS_IDLE;
	    IDSetSwitch(&MovementSP, NULL);
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
	      	
	      IUResetSwitches(&MovementSP);
	      MovementSP.s       = IPS_IDLE;
	      IDSetSwitch(&MovementSP, "Slew aborted.");
	      return;
	   }
	   
	   // East/West movement is illegal
	   if (lastMove[LX200_EAST] && lastMove[LX200_WEST])	
	   {
	      abortSlew();
	      for (int i=0; i < 4; i++)
	            lastMove[i] = 0;
	       
	      IUResetSwitches(&MovementSP);
     	      MovementSP.s       = IPS_IDLE;
	      IDSetSwitch(&MovementSP, "Slew aborted.");
	      return;
	   }
	      
          IDLog("We have switch %d \n ", index);
	  IDLog("NORTH: %d -- WEST: %d -- EAST: %d -- SOUTH %d\n", lastMove[0], lastMove[1], lastMove[2], lastMove[3]);

	  if (lastMove[index] == 1)
	  {
	        IDLog("issuing a move command\n");
	    	if ( ( err = MoveTo(index) < 0) )
	  	{
	        	 handleError(&MovementSP, err, "Setting motion direction");
 		 	return;
	  	}
	  }
	  else
	     HaltMovement(index);

          if (!lastMove[0] && !lastMove[1] && !lastMove[2] && !lastMove[3])
	     MovementSP.s = IPS_IDLE;
	  
	  if (lastMove[index] == 0)
	     IDSetSwitch(&MovementSP, "Moving toward %s aborted.", Direction[index]);
	  else
	  {
	     MovementSP.s = IPS_BUSY;
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
	     
	     IDSetSwitch(&MovementSP, "Moving %s...", combinedDir);
	  }
	  return;
	}

	// Tracking mode
	if (!strcmp (name, TrackModeSP.name))
	{
	  if (checkPower(&TrackModeSP))
	   return;

	  IUResetSwitches(&TrackModeSP);
	  IUUpdateSwitches(&TrackModeSP, states, names, n);
	  trackingMode = getOnSwitch(&TrackModeSP);
	  
	  if ( ( err = selectAPTrackingMode(trackingMode) < 0) )
	  {
	         handleError(&TrackModeSP, err, "Setting tracking mode.");
		 return;
	  }
	  
          
	  TrackModeSP.s = IPS_OK;
	  IDSetSwitch(&TrackModeSP, NULL);
	  return;
	}

}

void APMount::handleError(ISwitchVectorProperty *svp, int err, const char *msg)
{
  
  svp->s = IPS_ALERT;
  
  /* First check to see if the telescope is connected */
    if (testAP())
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

void APMount::handleError(INumberVectorProperty *nvp, int err, const char *msg)
{
  
  nvp->s = IPS_ALERT;
  
  /* First check to see if the telescope is connected */
    if (testAP())
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

void APMount::handleError(ITextVectorProperty *tvp, int err, const char *msg)
{
  
  tvp->s = IPS_ALERT;
  
  /* First check to see if the telescope is connected */
    if (testAP())
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

 void APMount::correctFault()
 {
 
   fault = false;
   IDMessage(mydev, "Telescope is online.");
   
 }

bool APMount::isTelescopeOn(void)
{
  if (simulation) return true;
  
  return (PowerSP.sp[0].s == ISS_ON);
}

static void retryConnection(void * p)
{
  p=p;
  
  if (testAP())
	telescope->connectionLost();
  else
	telescope->connectionResumed();
}

void APMount::ISPoll()
{
        double dx, dy;
	int err=0;
	
	if (!isTelescopeOn())
	 return;

	switch (EqNP.s)
	{
	case IPS_IDLE:
	getLX200RA(&currentRA);
	getLX200DEC(&currentDEC);
	
        if ( fabs (currentRA - lastRA) > 0.01 || fabs (currentDEC - lastDEC) > 0.01)
	{
	        lastRA = currentRA;
		lastDEC = currentDEC;
		IDSetNumber (&EqNP, NULL);
	}
        break;

        case IPS_BUSY:
	    getLX200RA(&currentRA);
	    getLX200DEC(&currentDEC);
	    dx = targetRA - currentRA;
	    dy = targetDEC - currentDEC;

	    IDLog("targetRA is %g, currentRA is %g\n", targetRA, currentRA);
	    IDLog("targetDEC is %g, currentDEC is %g\n*************************\n", targetDEC, currentDEC);

	    // Wait until acknowledged or within threshold
	    if (fabs(dx) <= RA_THRESHOLD && fabs(dy) <= DEC_THRESHOLD)
	    {
		
	       lastRA  = currentRA;
	       lastDEC = currentDEC;
	       IUResetSwitches(&OnCoordSetSP);
	       OnCoordSetSP.s = IPS_OK;
	       EqNP.s = IPS_OK;
	       IDSetNumber (&EqNP, NULL);

		switch (currentSet)
		{
		  case LX200_SLEW:
		  	OnCoordSetSP.sp[0].s = ISS_ON;
		  	IDSetSwitch (&OnCoordSetSP, "Slew is complete.");
		  	break;
		  
		  case LX200_TRACK:
		  	OnCoordSetSP.sp[1].s = ISS_ON;
		  	IDSetSwitch (&OnCoordSetSP, "Slew is complete. Tracking...");
			break;
		  
		  case LX200_SYNC:
		  	break;
		}
		  
	    } else
		IDSetNumber (&EqNP, NULL);
	    break;

	case IPS_OK:
	  
	if ( (err = getLX200RA(&currentRA)) < 0 || (err = getLX200DEC(&currentDEC)) < 0)
	{
	  handleError(&EqNP, err, "Getting RA/DEC");
	  return;
	}
	
	if (fault)
	  correctFault();
	
	if ( (currentRA != lastRA) || (currentDEC != lastDEC))
	{
	  	lastRA  = currentRA;
		lastDEC = currentDEC;
		IDSetNumber (&EqNP, NULL);
	}
        break;


	case IPS_ALERT:
	    break;
	}

	switch (MovementSP.s)
	{
	  case IPS_IDLE:
	   break;
	 case IPS_BUSY:
	   getLX200RA(&currentRA);
	   getLX200DEC(&currentDEC);
	   IDSetNumber (&EqNP, NULL);
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
         
	      
	      FocusMotionSP.s = IPS_IDLE;
	      FocusTimerNP.s  = IPS_OK;
	      FocusSpeedNP.s  = IPS_OK;
	      
              IUResetSwitches(&FocusMotionSP);
	      FocusSpeedN[0].value = 0;
	      
	      IDSetNumber(&FocusSpeedNP, NULL);
	      IDSetSwitch(&FocusMotionSP, NULL);
	    }
	    
	    IDSetNumber(&FocusTimerNP, NULL);
	    break;
	    
	   case IPS_OK:
	    break;
	    
	   case IPS_ALERT:
	    break;
	 }
    

}

void APMount::getBasicData()
{

  // #1 Save current time
  IUSaveText(&UTCT[0], timestamp());
  IDLog("PC UTC time is %s\n", UTCT[0].text);

  // #2 Make sure format is long
  checkLX200Format();
  timeFormat = LX200_24;

  // #3 Get current RA/DEC
  getLX200RA(&currentRA);
  getLX200DEC(&currentDEC);
  targetRA = currentRA;
  targetDEC = currentDEC;

  IDSetNumber (&EqNP, NULL);  
  updateLocation();
  updateTime();
  
}

int APMount::handleCoordSet()
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
	  if (EqNP.s == IPS_BUSY)
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

	  EqNP.s = IPS_BUSY;
	  fs_sexa(RAStr, targetRA, 2, 3600);
	  fs_sexa(DecStr, targetDEC, 2, 3600);
	  IDSetNumber(&EqNP, "Slewing to JNow RA %s - DEC %s", RAStr, DecStr);
	  IDLog("Slewing to JNow RA %s - DEC %s\n", RAStr, DecStr);
	  break;

     // Track
     case LX200_TRACK:
          IDLog("We're in LX200_TRACK\n");
          if (EqNP.s == IPS_BUSY)
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
		EqNP.s = IPS_BUSY;
		IDSetNumber(&EqNP, "Slewing to JNow RA %s - DEC %s", RAStr, DecStr);
		IDLog("Slewing to JNow RA %s - DEC %s\n", RAStr, DecStr);
	  }
	  else
	  {
	    IDLog("Tracking called, but tracking threshold not reached yet.\n");
	    EqNP.s = IPS_OK;
	    EqNP.np[0].value = currentRA;
	    EqNP.np[1].value = currentDEC;

	    if (lastSet != LX200_TRACK)
	      IDSetNumber(&EqNP, "Tracking...");
	    else
	      IDSetNumber(&EqNP, NULL);
	  }
	  lastSet = LX200_TRACK;
      break;

    // Sync
    case LX200_SYNC:
          lastSet = LX200_SYNC;
	  EqNP.s = IPS_IDLE;
	   
	  if ( ( err = Sync(syncString) < 0) )
	  {
	        IDSetNumber( &EqNP , "Synchronization failed.");
		return (-1);
	  }

	  EqNP.s = IPS_OK;
	  IDLog("Synchronization successful %s\n", syncString);
	  IDSetNumber(&EqNP, "Synchronization successful.");
	  break;
    }

   return (0);

}

int APMount::getOnSwitch(ISwitchVectorProperty *sp)
{
 for (int i=0; i < sp->nsp ; i++)
     if (sp->sp[i].s == ISS_ON)
      return i;

 return -1;
}


int APMount::checkPower(ISwitchVectorProperty *sp)
{
  if (simulation) return 0;
  
  if (PowerSP.s != IPS_OK)
  {
    if (!strcmp(sp->label, ""))
    	IDMessage (mydev, "Cannot change property %s while the telescope is offline.", sp->name);
    else
        IDMessage (mydev, "Cannot change property %s while the telescope is offline.", sp->label);
	
    sp->s = IPS_IDLE;
    IDSetSwitch(sp, NULL);
    return -1;
  }

  return 0;
}

int APMount::checkPower(INumberVectorProperty *np)
{
  if (simulation) return 0;
  
  if (PowerSP.s != IPS_OK)
  {
    
    if (!strcmp(np->label, ""))
    	IDMessage (mydev, "Cannot change property %s while the telescope is offline.", np->name);
    else
        IDMessage (mydev, "Cannot change property %s while the telescope is offline.", np->label);
	
    np->s = IPS_IDLE;
    IDSetNumber(np, NULL);
    return -1;
  }

  return 0;

}

int APMount::checkPower(ITextVectorProperty *tp)
{

  if (simulation) return 0;
  
  if (PowerSP.s != IPS_OK)
  {
    if (!strcmp(tp->label, ""))
    	IDMessage (mydev, "Cannot change property %s while the telescope is offline.", tp->name);
    else
        IDMessage (mydev, "Cannot change property %s while the telescope is offline.", tp->label);
	
    tp->s = IPS_IDLE;
    IDSetText(tp, NULL);
    return -1;
  }

  return 0;

}

void APMount::powerTelescope()
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
	
         if (Connect(PortT[0].text))
	 {
	   PowerS[0].s = ISS_OFF;
	   PowerS[1].s = ISS_ON;
	   IDSetSwitch (&PowerSP, "Error connecting to port %s\n", PortT[0].text);
	   return;
	 }

	 if (testAP())
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

void APMount::slewError(int slewCode)
{
    OnCoordSetSP.s = IPS_IDLE;
    ParkSP.s = IPS_IDLE;
    IDSetSwitch(&ParkSP, NULL);

    if (slewCode == 1)
	IDSetSwitch (&OnCoordSetSP, "Object below horizon.");
    else if (slewCode == 2)
	IDSetSwitch (&OnCoordSetSP, "Object below the minimum elevation limit.");
    else
        IDSetSwitch (&OnCoordSetSP, "Slew failed.");
    

}

void APMount::enableSimulation(bool enable)
{
   simulation = enable;
   
   if (simulation)
     IDLog("Warning: Simulation is activated.\n");
   else
     IDLog("Simulation is disabled.\n");
}

void APMount::updateTime()
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
    sprintf(UTCT[0].text, "%d-%02d-%02dT%02d:%02d:%02d", 1979, 6, 25, 3, 30, 30);
    IDLog("Telescope ISO date and time: %s\n", UTCT[0].text);
    IDSetText(&TimeTP, NULL);
    return;
  }
  
  getLocalTime24(&ctime);
  getSexComponents(ctime, &h, &m, &s);
  
  UTC_h = h;
  
  if ( (result = getSDTime(&SDTimeN[0].value)) < 0)
    IDMessage(mydev, "Failed to retrieve siderial time from device.");
  
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
  sprintf(UTCT[0].text, "%d-%02d-%02dT%02d:%02d:%02d", UTC_year, UTC_month, UTC_day, UTC_h, m, s);
  
  IDLog("Local telescope time: %02d:%02d:%02d\n", h, m , s);
  IDLog("Telescope SD Time is: %g\n", SDTimeN[0].value);
  IDLog("UTC date and time: %s\n", UTCT[0].text);
  

  // Let's send everything to the client
  IDSetText(&TimeTP, NULL);
  IDSetNumber(&SDTimeNP, NULL);

}

void APMount::updateLocation()
{

 int dd = 0, mm = 0, err = 0;
 
 if ( (err = getSiteLatitude(&dd, &mm)) < 0)
    IDMessage(mydev, "Failed to get site latitude from device.");
  else
  {
    if (dd > 0)
    	GeoNP.np[0].value = dd + mm/60.0;
    else
        GeoNP.np[0].value = dd - mm/60.0;
  
      IDLog("Astro-Physics Latitude: %d:%d\n", dd, mm);
      IDLog("INDI Latitude: %g\n", GeoNP.np[0].value);
  }
  
  if ( (err = getSiteLongitude(&dd, &mm)) < 0)
    IDMessage(mydev, "Failed to get site longitude from device.");
  else
  {
    if (dd > 0) GeoNP.np[1].value = 360.0 - (dd + mm/60.0);
    else GeoNP.np[1].value = (dd - mm/60.0) * -1.0;
    
    IDLog("Astro-Physics Longitude: %d:%d\n", dd, mm);
    IDLog("INDI Longitude: %g\n", GeoNP.np[1].value);
  }
  
  IDSetNumber (&GeoNP, NULL);

}

void APMount::connectionLost()
{
    PowerSP.s = IPS_IDLE;
    IDSetSwitch(&PowerSP, "The connection to the telescope is lost.");
    return;
 
}

void APMount::connectionResumed()
{
  PowerS[0].s = ISS_ON;
  PowerS[1].s = ISS_OFF;
  PowerSP.s = IPS_OK;
   
  IDSetSwitch(&PowerSP, "The connection to the telescope has been resumed.");
}


