/*
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

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lx200GPS.h"
#include "lx200driver.h"


extern LX200Generic *telescope;
extern char mydev[];
extern int MaxReticleFlashRate;

static ISwitch GPSPowerS[]		= {{ "On", ISS_OFF}, {"Off", ISS_ON}};
static ISwitch GPSStatusS[]	  	= {{ "GPS Sleep", ISS_OFF}, {"GPS Wake up", ISS_OFF}, {"GPS Restart", ISS_OFF}};
static ISwitch GPSUpdateS[]	  	= { {"Update GPS system", ISS_OFF}};
static ISwitch AltDecPecS[]		= {{ "Enable", ISS_OFF}, {"Disable", ISS_OFF}};
static ISwitch AzRaPecS[]		= {{ "Enable", ISS_OFF}, {"Disable", ISS_OFF}};
static ISwitch SelenSyncS[]		= {{ "Selenographic Sync", ISS_OFF}};
static ISwitch AltDecBackSlashS[]	= {{ "Activate Alt Dec Anti Backslash", ISS_OFF}};
static ISwitch AzRaBackSlashS[]		= {{ "Activate Az Ra Anti Backslash", ISS_OFF}};

static ISwitches GPSPowerSw		= { mydev, "GPS Power", GPSPowerS, NARRAY(GPSPowerS), ILS_IDLE, 0};
static ISwitches GPSStatusSw		= { mydev, "GPS Status", GPSStatusS, NARRAY(GPSStatusS), ILS_IDLE, 0};
static ISwitches GPSUpdateSw		= { mydev, "GPS Update", GPSUpdateS, NARRAY(GPSUpdateS), ILS_IDLE, 0};
static ISwitches AltDecPecSw		= { mydev, "Alt/Dec PEC Compensation", AltDecPecS, NARRAY(AltDecPecS), ILS_IDLE, 0};
static ISwitches AzRaPecSw		= { mydev, "Az/Ra PEC Compensation", AzRaPecS, NARRAY(AzRaPecS), ILS_IDLE, 0};
static ISwitches SelenSyncSw		= { mydev, "Selenographic Sync", SelenSyncS, NARRAY(SelenSyncS), ILS_IDLE, 0};
static ISwitches AltDecBackSlashSw	= { mydev, "Alt/Dec Anti-backslash", AltDecBackSlashS, NARRAY(AltDecBackSlashS), ILS_IDLE, 0};
static ISwitches AzRaBackSlashSw	= { mydev, "Az/Ra Anti-backslash", AzRaBackSlashS, NARRAY(AzRaBackSlashS), ILS_IDLE, 0};

static IText OTATemp			= { mydev, "OTA Temperature",  NULL, ILS_IDLE};



LX200GPS::LX200GPS() : LX200_16()
{

  OTATemp.text = new char[8];
}

void LX200GPS::ISGetProperties (char *dev)
{

if (dev && strcmp (mydev, dev))
    return;

// process parent first
   LX200_16::ISGetProperties(dev);

ICDefSwitches (&GPSPowerSw, "GPS Power", ISP_W, IR_1OFMANY);
ICDefSwitches (&GPSStatusSw, "GPS Status", ISP_W, IR_1OFMANY);
ICDefSwitches (&GPSUpdateSw, "GPS Update", ISP_W, IR_1OFMANY);
ICDefSwitches (&AltDecPecSw, "Alt/Dec PEC Compensation", ISP_W, IR_1OFMANY);
ICDefSwitches (&AzRaPecSw, "Az/Ra PEC Compensation", ISP_W, IR_1OFMANY);
ICDefSwitches (&SelenSyncSw, "Selengraphic Sync", ISP_W, IR_1OFMANY);
ICDefSwitches (&AltDecBackSlashSw, "Alt/Dec Anti-backslash", ISP_W, IR_1OFMANY);
ICDefSwitches (&AzRaBackSlashSw, "Az/Ra Anti-backslash", ISP_W, IR_1OFMANY);
ICDefText     (&OTATemp, "OTA Temperature", IP_RO);


}

void LX200GPS::ISNewText (IText *t)
{
     LX200_16::ISNewText(t);

}

void LX200GPS::ISNewNumber (INumber *n)
{
    LX200_16::ISNewNumber(n);

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


 void LX200GPS::ISNewSwitch (ISwitches *s)
 {
    int index[16];
    char msg[32];

    LX200_16::ISNewSwitch(s);

    if (strcmp (s->dev, mydev))
	    return;

    if (!validateSwitch(s, &GPSPowerSw, NARRAY(GPSPowerS), index, 1))
	{
	  index[0] == 0 ? turnGPSOn() : turnGPSOff();
	  GPSPowerSw.s = ILS_OK;
	  strcpy(msg, index[0] == 0 ? "GPS System is ON" : "GPS System is OFF");
	  ICSetSwitch (&GPSPowerSw, msg);
	  return;
	}

    if (!validateSwitch(s, &GPSStatusSw, NARRAY(GPSStatusS), index, 1))
	{
	  if (index[0] == 0)
	  {
	   gpsSleep();
	   strcpy(msg, "GPS system is in sleep mode");
	  }
	  else if (index[0] == 1)
	  {
	   gpsWakeUp();
           strcpy(msg, "GPS system is reactivated");
	  }
	  else
	  {
	   gpsRestart();
	   strcpy(msg, "GPS system is restarting...");
	  }

	  GPSStatusSw.s = ILS_OK;
	  ICSetSwitch (&GPSStatusSw, msg);
	  return;
	}


   if (!validateSwitch(s, &GPSUpdateSw, NARRAY(GPSUpdateS), index, 1))
   {
     GPSUpdateSw.s = ILS_OK;
     ICSetSwitch(&GPSUpdateSw, "Updating GPS system. This operation might take few minutes to complete...");
     if (updateGPS_System())
     	ICSetSwitch(&GPSUpdateSw, "GPS system update successful");
     else
        ICSetSwitch(&GPSUpdateSw, "GPS system update failed");

     return;
   }

   if (!validateSwitch(s, &AltDecPecSw, NARRAY(AltDecPecS), index, 1))
   {
      if (index[0] == 0)
      {
        enableDecAltPec();
	strcpy (msg, "Alt/Dec Compensation Enabled");
      }
      else
      {
        disableDecAltPec();
	strcpy (msg, "Alt/Dec Compensation Disabled");
      }

      AltDecPecSw.s = ILS_OK;
      ICSetSwitch(&AltDecPecSw, msg);
      return;
    }

   if (!validateSwitch(s, &AzRaPecSw, NARRAY(AzRaPecS), index, 1))
   {
      if (index[0] == 0)
      {
        enableRaAzPec();
	strcpy (msg, "Ra/Az Compensation Enabled");
      }
      else
      {
        disableRaAzPec();
	strcpy (msg, "Ra/Az Compensation Disabled");
      }

      AzRaPecSw.s = ILS_OK;
      ICSetSwitch(&AzRaPecSw, msg);
      return;
    }

   if (!validateSwitch(s, &AltDecBackSlashSw, NARRAY(AltDecBackSlashS), index, 1))
   {

     activateAltDecAntiBackSlash();
     AltDecBackSlashSw.s = ILS_OK;
     ICSetSwitch(&AltDecBackSlashSw, "Alt/Dec Anti-backslash enabled");
     return;
   }

   if (!validateSwitch(s, &AzRaBackSlashSw, NARRAY(AzRaBackSlashS), index, 1))
   {

     activateAzRaAntiBackSlash();
     AzRaBackSlashSw.s = ILS_OK;
     ICSetSwitch(&AzRaBackSlashSw, "Az/Ra Anti-backslash enabled");
     return;
   }




}

 void LX200GPS::ISPoll ()
 {

   LX200_16::ISPoll();

 }

 void LX200GPS::getBasicData()
 {

   // process parent first
   LX200_16::getBasicData();

   sprintf(OTATemp.text, "%+02.1fº C", (float) getOTATemp());
   ICSetText(&OTATemp, NULL);
 }

