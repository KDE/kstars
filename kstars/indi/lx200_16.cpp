/*
    LX200 16"
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

#include "lx200_16.h"
#include "lx200driver.h"


#define LX16Group	"GPS/16 inch Features"
extern LX200Generic *telescope;
extern char mydev[];
extern int MaxReticleFlashRate;


//void turnFanOn();
   //void turnFanOff();

   //void seekHomeAndSave();
  // void seekHomeAndSet();

   /** Turns the field derotator On or Off. This command applies <b>only</b> to 16" LX200.
   @param turnOn if turnOn is true, the derotator is turned on, otherwise, it is turned off. */
 //  void fieldDeRotator(bool turnOn);


   /** Sets object Altitude. \n
   @returns true if object's altitude is successfully set. */
    //bool setObjAlt(int degrees, int minutes);

   /** Sets object Azimuth. \n
   @returns true if object's azimuth is successfully set. */
//    bool setObjAz(int degrees, int minutes);


static ISwitch FanStatusS[]		= { {"On", ISS_OFF}, {"Off", ISS_OFF}};
static ISwitch HomeSearchS[]		= { {"Seek home and save", ISS_OFF} , {"Seek home and set", ISS_ON}};
static ISwitch FieldDeRotatorS[]	= { {"On", ISS_OFF}, {"Off", ISS_OFF}};
static ISwitch SlewAltAzS[]		= { {"Slew to Object Az/Alt", ISS_OFF}};

static ISwitches FanStatusSw		= { mydev, "Fan", FanStatusS, NARRAY(FanStatusS), ILS_IDLE, 0, LX16Group };
static ISwitches HomeSearchSw		= { mydev, "Home Search", HomeSearchS, NARRAY(HomeSearchS), ILS_IDLE, 0, LX16Group};
static ISwitches FieldDeRotatorSw	= { mydev, "Field De-rotator", FieldDeRotatorS, NARRAY(FieldDeRotatorS), ILS_IDLE, 0, LX16Group};
static ISwitches SlewAltAzSw		= { mydev, "Slew to Object Az/Alt", SlewAltAzS, NARRAY(SlewAltAzS), ILS_IDLE, 0, LX16Group};

static INumber ObjectAlt		= { mydev, "Object Altitude", NULL, ILS_IDLE, 0, LX16Group};
static INumber ObjectAz			= { mydev, "Object Azimuth", NULL, ILS_IDLE, 0, LX16Group};


LX200_16::LX200_16() : LX200Autostar()
{

 ObjectAlt.nstr	= strcpy( new char[9], "DD:MM");
 ObjectAz.nstr	= strcpy( new char[9], "DDD:MM");

}

void LX200_16::ISGetProperties (const char *dev)
{

if (dev && strcmp (mydev, dev))
    return;

 // process parent first
  LX200Autostar::ISGetProperties(dev);

  ICDefNumber (&ObjectAlt, "Object Alt", IP_RW, NULL);
  ICDefNumber (&ObjectAz , "Object Az", IP_RW, NULL);

  ICDefSwitches(&SlewAltAzSw, "Az/Alt Slew", ISP_W, IR_1OFMANY);

  ICDefSwitches (&FanStatusSw, "Fan", ISP_W, IR_1OFMANY);
  ICDefSwitches (&HomeSearchSw, "Home Search", ISP_W, IR_1OFMANY);
  ICDefSwitches (&FieldDeRotatorSw, "Field deRotator", ISP_W, IR_1OFMANY);

}

void LX200_16::ISNewText (IText *t)
{

   LX200Autostar::ISNewText(t);

}

void LX200_16::ISNewNumber (INumber *n)
{
  int h,m;

  LX200Autostar::ISNewNumber(n);

  // ignore if not ours //
  if (strcmp (n->dev, mydev))
    return;

  if ( !strcmp (n->name, ObjectAz.name) )
	{
	  if (checkPower())
	  {
	    ObjectAz.s = ILS_IDLE;
	    ICSetNumber(&ObjectAz, NULL);
	    return;
	  }

	  if (!sscanf(n->nstr, "%d:%d", &h, &m) || m > 59 || m < 0 || h < 0 || h > 360)
	  {
	    ObjectAz.s = ILS_IDLE;
	    ICSetNumber(&ObjectAz , "Coordinates invalid");
	    return;
	  }

          setObjAz(h, m);
	  ObjectAz.s = ILS_OK;
	  strcpy(ObjectAz.nstr , n->nstr);
	  ICSetNumber(&ObjectAz , "Object Azimuth set to %d:%d", h, m);
	  return;
        }

	if ( !strcmp (n->name, ObjectAlt.name) )
	{
	  if (checkPower())
	  {
	    ObjectAlt.s = ILS_IDLE;
	    ICSetNumber(&ObjectAlt, NULL);
	    return;
	  }

	  if (!sscanf(n->nstr, "%d:%d", &h, &m) || m > 59 || m < 0 || h < -90 || h > 90)
	  {
	    ObjectAlt.s = ILS_IDLE;
	    ICSetNumber(&ObjectAlt , "Coordinates invalid");
	    return;
	  }

          setObjAlt(h, m);
	  ObjectAlt.s = ILS_OK;
	  strcpy(ObjectAlt.nstr , n->nstr);
	  ICSetNumber(&ObjectAlt , "Object Altitude set to %d:%d", h, m);
	  return;
        }

 }

void LX200_16::ISNewSwitch (ISwitches *s)
{
   int index[16], i;
   char msg[32];

  if (strcmp (s->dev, mydev))
    return;

   // process parent
   LX200Autostar::ISNewSwitch(s);

   if (!validateSwitch(s, &FanStatusSw, NARRAY(FanStatusS), index, 1))
   {
	  index[0] == 0 ? turnFanOn() : turnFanOff();
	  FanStatusSw.s = ILS_OK;
	  strcpy(msg, index[0] == 0 ? "Fan is ON" : "Fan is Off");
	  ICSetSwitch (&FanStatusSw, msg);
	  return;
   }

   if (!validateSwitch(s, &HomeSearchSw, NARRAY(HomeSearchS), index, 1))
   {
	  index[0] == 0 ? seekHomeAndSave() : seekHomeAndSet();
	  HomeSearchSw.s = ILS_BUSY;
	  ICSetSwitch (&HomeSearchSw, msg);
	  return;
   }

   if (!validateSwitch(s, &FieldDeRotatorSw, NARRAY(FieldDeRotatorS), index, 1))
   {
	  index[0] == 0 ? turnFieldDeRotatorOn() : turnFieldDeRotatorOff();
	  FanStatusSw.s = ILS_OK;
	  strcpy(msg, index[0] == 0 ? "Field deRotator is ON" : "Field deRotator is Off");
	  ICSetSwitch (&FanStatusSw, msg);
	  return;
   }

   if (!validateSwitch(s, &SlewAltAzSw, NARRAY(SlewAltAzS), index, 1))
   {
     if (checkPower())
	  {
	    SlewAltAzSw.s = ILS_IDLE;
	    ICSetSwitch(&SlewAltAzSw, NULL);
	    return;
	  }

	  if (SlewAltAzSw.s == ILS_BUSY)
	  {
	     abortSlew();

	     // sleep for 100 mseconds
	     usleep(100000);
	  }

	  if ((i = slewToAltAz()))
	  {
            SlewAltAzSw.s = ILS_IDLE;
	    ICSetSwitch(&SlewAltAzSw, "Slew not possible");
	    return;
	  }

	  // TODO
	  // We need to convert Object's Alt/Az to RA/DEC because we can't get Object's
	  // Alt/Az directly from the telescope. Without this, we can't update and check the
	  // slew status
	  SlewAltAzSw.s = ILS_OK;
	  ICSetSwitch(&SlewAltAzSw, "Slewing to Az %s - Alt %s", ObjectAz.nstr, ObjectAlt.nstr);

	  return;
	}

}

 void LX200_16::ISPoll ()
 {
   int searchResult;
   LX200Autostar::ISPoll();

   	switch (HomeSearchSw.s)
	{
	case ILS_IDLE:
	     break;

	case ILS_BUSY:

	    searchResult = getHomeSearchStatus();

	    if (!searchResult)
	    {
	      HomeSearchSw.s = ILS_IDLE;
	      ICSetSwitch(&HomeSearchSw, "Home search failed");
	    }
	    else if (searchResult == 1)
	    {
	      HomeSearchSw.s = ILS_OK;
	      ICSetSwitch(&HomeSearchSw, "Home search successful");
	    }
	    else if (searchResult == 2)
	      ICSetSwitch(&HomeSearchSw, "Home search in progress...");
	    else
	    {
	      HomeSearchSw.s = ILS_IDLE;
	      ICSetSwitch(&HomeSearchSw, "Home search error");
	    }
	    break;

	 case ILS_OK:
	   break;
	 case ILS_ALERT:
	  break;
	}

 }

 void LX200_16::getBasicData()
 {
   // process parent first
   LX200Autostar::getBasicData();

 }
