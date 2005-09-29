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
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "lx200gps.h"
#include "lx200driver.h"

#define GPSGroup   "Extended GPS Features"

extern LX200Generic *telescope;
extern int MaxReticleFlashRate;

static ISwitch GPSPowerS[]		= {{ "On", "", ISS_OFF, 0, 0}, {"Off", "", ISS_ON, 0, 0}};
static ISwitch GPSStatusS[]	  	= {{ "Sleep", "", ISS_OFF, 0, 0}, {"Wake up", "", ISS_OFF, 0 ,0}, {"Restart", "", ISS_OFF, 0, 0}};
static ISwitch GPSUpdateS[]	  	= { {"Update", "", ISS_OFF, 0, 0}};
static ISwitch AltDecPecS[]		= {{ "Enable", "", ISS_OFF, 0 ,0}, {"Disable", "", ISS_OFF, 0 ,0}};
static ISwitch AzRaPecS[]		= {{ "Enable", "", ISS_OFF, 0, 0}, {"Disable", "", ISS_OFF, 0 ,0}};
static ISwitch SelenSyncS[]		= {{ "Sync", "",  ISS_OFF, 0, 0}};
static ISwitch AltDecBackSlashS[]	= {{ "Activate", "", ISS_OFF, 0, 0}};
static ISwitch AzRaBackSlashS[]		= {{ "Activate", "", ISS_OFF, 0, 0}};
static ISwitch OTAUpdateS[]		= {{ "Update", "", ISS_OFF, 0, 0}};

static ISwitchVectorProperty GPSPowerSw	   = { mydev, "GPS Power", "", GPSGroup, IP_RW, ISR_1OFMANY, 0 , IPS_IDLE, GPSPowerS, NARRAY(GPSPowerS), "", 0};
static ISwitchVectorProperty GPSStatusSw   = { mydev, "GPS Status", "", GPSGroup, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, GPSStatusS, NARRAY(GPSStatusS), "", 0};
static ISwitchVectorProperty GPSUpdateSw   = { mydev, "GPS System", "", GPSGroup, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, GPSUpdateS, NARRAY(GPSUpdateS), "", 0};
static ISwitchVectorProperty AltDecPecSw   = { mydev, "Alt/Dec PEC", "", GPSGroup, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, AltDecPecS, NARRAY(AltDecPecS), "", 0};
static ISwitchVectorProperty AzRaPecSw	   = { mydev, "Az/Ra PEC", "", GPSGroup, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, AzRaPecS, NARRAY(AzRaPecS), "", 0};
static ISwitchVectorProperty SelenSyncSw   = { mydev, "Selenographic Sync", "", GPSGroup, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, SelenSyncS, NARRAY(SelenSyncS), "", 0};
static ISwitchVectorProperty AltDecBackSlashSw	= { mydev, "Alt/Dec Anti-backslash", "", GPSGroup, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, AltDecBackSlashS, NARRAY(AltDecBackSlashS), "", 0};
static ISwitchVectorProperty AzRaBackSlashSw	= { mydev, "Az/Ra Anti-backslash", "", GPSGroup, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, AzRaBackSlashS, NARRAY(AzRaBackSlashS), "", 0};
static ISwitchVectorProperty OTAUpdateSw	= { mydev, "OTA Update", "", GPSGroup, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, OTAUpdateS, NARRAY(OTAUpdateS), "", 0};

static INumber Temp[]	= { {"Temp.", "", "%g", -200., 500., 0., 0., 0, 0, 0 } };
static INumberVectorProperty OTATemp =   { mydev, "OTA Temperature (C)", "", GPSGroup, IP_RO, 0, IPS_IDLE, Temp, NARRAY(Temp), "", 0};

void updateTemp(void * /*p*/);

void changeLX200GPSDeviceName(const char *newName)
{
 strcpy(GPSPowerSw.device, newName);
 strcpy(GPSStatusSw.device, newName );
 strcpy(GPSUpdateSw.device, newName  );
 strcpy(AltDecPecSw.device, newName );
 strcpy(AzRaPecSw.device,newName  );
 strcpy(SelenSyncSw.device, newName );
 strcpy(AltDecBackSlashSw.device, newName );
 strcpy(AzRaBackSlashSw.device, newName );
 strcpy(OTATemp.device, newName );
 strcpy(OTAUpdateSw.device, newName);
 
}

LX200GPS::LX200GPS() : LX200_16()
{
   IEAddTimer(900000, updateTemp, NULL);
   
}

void LX200GPS::ISGetProperties (const char *dev)
{

if (dev && strcmp (thisDevice, dev))
    return;

// process parent first
   LX200_16::ISGetProperties(dev);

IDDefSwitch (&GPSPowerSw, NULL);
IDDefSwitch (&GPSStatusSw, NULL);
IDDefSwitch (&GPSUpdateSw, NULL);
IDDefSwitch (&AltDecPecSw, NULL);
IDDefSwitch (&AzRaPecSw, NULL);
IDDefSwitch (&SelenSyncSw, NULL);
IDDefSwitch (&AltDecBackSlashSw, NULL);
IDDefSwitch (&AzRaBackSlashSw, NULL);
IDDefNumber (&OTATemp, NULL);
IDDefSwitch (&OTAUpdateSw, NULL);

}

void LX200GPS::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
	// ignore if not ours //
	if (strcmp (dev, thisDevice))
	    return;

     LX200_16::ISNewText (dev, name, texts, names, n);
}

void LX200GPS::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
        
    LX200_16::ISNewNumber (dev, name, values, names, n);

 }


 void LX200GPS::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
 {
    int index;
    char msg[64];

    if (strcmp (dev, thisDevice))
	    return;

    /* GPS Power */
    if (!strcmp(name,GPSPowerSw.name))
    {
       if (checkPower(&GPSPowerSw))
       return;

      IUResetSwitches(&GPSPowerSw);
      IUUpdateSwitches(&GPSPowerSw, states, names, n);
      index = getOnSwitch(&GPSPowerSw);
      index == 0 ? turnGPSOn() : turnGPSOff();
      GPSPowerSw.s = IPS_OK;
      IDSetSwitch (&GPSPowerSw, index == 0 ? "GPS System is ON" : "GPS System is OFF" );
      return;
    }

    /* GPS Status Update */
    if (!strcmp(name,GPSStatusSw.name))
    {
       if (checkPower(&GPSStatusSw))
       return;

      IUResetSwitches(&GPSStatusSw);
      IUUpdateSwitches(&GPSStatusSw, states, names, n);
      index = getOnSwitch(&GPSStatusSw);

      if (index == 0)
      {
	   gpsSleep();
	   strcpy(msg, "GPS system is in sleep mode.");
      }
      else if (index == 1)
      {
	   gpsWakeUp();
           strcpy(msg, "GPS system is reactivated.");
      }
      else
      {
	   gpsRestart();
	   strcpy(msg, "GPS system is restarting...");
	   updateTime();
	   updateLocation();
      }

	GPSStatusSw.s = IPS_OK;
	IDSetSwitch (&GPSStatusSw, "%s", msg);
	return;

    }

    /* GPS Update */
    if (!strcmp(name,GPSUpdateSw.name))
    {
       if (checkPower(&GPSUpdateSw))
       return;

     GPSUpdateSw.s = IPS_OK;
     IDSetSwitch(&GPSUpdateSw, "Updating GPS system. This operation might take few minutes to complete...");
     if (updateGPS_System())
     {
     	IDSetSwitch(&GPSUpdateSw, "GPS system update successful.");
	updateTime();
	updateLocation();
     }
     else
     {
        GPSUpdateSw.s = IPS_IDLE;
        IDSetSwitch(&GPSUpdateSw, "GPS system update failed.");
     }
     return;
    }

    /* Alt Dec Periodic Error correction */
    if (!strcmp(name, AltDecPecSw.name))
    {
       if (checkPower(&AltDecPecSw))
       return;

      IUResetSwitches(&AltDecPecSw);
      IUUpdateSwitches(&AltDecPecSw, states, names, n);
      index = getOnSwitch(&AltDecPecSw);
      
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
      IDSetSwitch(&AltDecPecSw, "%s", msg);

      return;
    }

    /* Az RA periodic error correction */
    if (!strcmp(name, AzRaPecSw.name))
    {
       if (checkPower(&AzRaPecSw))
       return; 

      IUResetSwitches(&AzRaPecSw);
      IUUpdateSwitches(&AzRaPecSw, states, names, n);
      index = getOnSwitch(&AzRaPecSw);

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
      IDSetSwitch(&AzRaPecSw, "%s", msg);

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
   
   if (!strcmp(name, OTAUpdateSw.name))
   {
     if (checkPower(&OTAUpdateSw))
      return;
      
      IUResetSwitches(&OTAUpdateSw);
      
      if (getOTATemp(&OTATemp.np[0].value) < 0)
      {
	OTATemp.s = IPS_ALERT;
	IDSetNumber(&OTATemp, "Error: OTA temperature read timed out.");
      }
      else
      {
	OTATemp.s = IPS_OK;
	IDSetNumber(&OTATemp, NULL);
      }
      
      return;
   }
      
     
   
   LX200_16::ISNewSwitch (dev, name, states, names,  n);

}

 void LX200GPS::ISPoll ()
 {

   LX200_16::ISPoll();


 }
 
 void updateTemp(void * /*p*/)
 {
   
   if (telescope->isTelescopeOn())
   {
     if (getOTATemp(&OTATemp.np[0].value) < 0)
     {
       OTATemp.s = IPS_ALERT;
       IDSetNumber(&OTATemp, "Error: OTA temperature read timed out.");
       return;
     }
     else
     {
        OTATemp.s = IPS_OK; 
   	IDSetNumber(&OTATemp, NULL);
     }
   } 
 
   IEAddTimer(900000, updateTemp, NULL);
      
 }

 void LX200GPS::getBasicData()
 {

   //getOTATemp(&OTATemp.np[0].value);
   //IDSetNumber(&OTATemp, NULL);
   
   // process parent
   LX200_16::getBasicData();
 }

