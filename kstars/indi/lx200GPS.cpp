#if 0
    LX200 GPS
    Copyright (C) 2003 Jasem Mutlaq

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

#include "lx200gps.h"
#include "lx200driver.h"


extern LX200Autostar *telescope;
extern char mydev[];

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
#define SelenographicSync()		portWrite("#:CL#")*/

static INumber Object_RA         = { mydev, "RA", NULL, ILS_IDLE};
static INumber Object_DEC        = { mydev, "DEC", NULL, ILS_IDLE};
static INumber LX200_RA          = { mydev, "LX200RA", NULL, ILS_IDLE};
static INumber LX200_DEC         = { mydev, "LX200DEC", NULL, ILS_IDLE};
static INumber UTCOffset         = { mydev, "UTC Offset", NULL, ILS_IDLE};
static IText   CalenderDate      = { mydev, "Calender Date", NULL, ILS_IDLE};

static ISwitch GPSPowerS	= {{ "On", ISS_OFF}, {"Off", ISS_ON}};
static ISwitch GPSStatusS	= {{ "GPS Sleep", ISS_OFF}, {"GPS Wake up", ISS_OFF}, {"GPS Restart", ISS_OFF}};
static ISwitch GPSUpdateS	= { {"Update GPS system", ISS_OFF}};
static ISwitch AltDecPecS	= {{ "Enable", ISS_OFF}, {"Disable", ISS_OFF}};
static ISwitch RaAzPecS		= {{ "Enable", ISS_OFF}, {"Disable", ISS_OFF}};
static ISwitch SelenSyncS	= {{ "Selenographic Sync", ISS_OFF}};


#ifdef LX200_GPS
void
ISInit()
{
	fprintf(stderr , "initilizaing from LX200 GPS device...\n");

	// Two important steps always performed when adding a sub-device

	// 1. mydev = device_name
	strcpy(mydev, "LX200GPS");
	// 2. device = sub_class
	telescope = new LX200GPS();

}
#endif

LX200GPS::LX200GPS() : LX200Autostar()
{


}

void LX200GPS::ISGetProperties (char *dev)
{

if (dev && strcmp (mydev, dev))
    return;

// process parent first
if (callParent)
   LX200Autostar::ISGetProperties(dev);
else
   callParent = 1;

}

void LX200GPS::ISNewText (IText *t)
{
 if (callParent)
    LX200Autostar::ISNewText(t);
 else
    callParent = 1;

}

void LX200GPS::ISNewNumber (INumber *n)
{
  if (callParent)
      LX200Autostar::ISNewNumber(n);
  else
     callParent = 1;

 }

 void LX200GPS::ISNewSwitch (ISwitches *s)
 {

    if (callParent)
       LX200Autostar::ISNewSwitch(s);
    else
       callParent = 1;



 }

 void LX200GPS::ISPoll ()
 {

   if (callParent)
      LX200Autostar::ISPoll();
   else
      callParent = 1;

 }

 void LX200GPS::getBasicData()
 {

   // process parent first
   if (callParent)
      LX200Autostar::getBasicData();
   else
      callParent;


 }

