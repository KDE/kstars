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
static ISwitch HomeSearchS[]		= { {"Seek home and save", ISS_OFF} , {"Seek home and set", ISS_OFF}};
static ISwitch FieldDeRotatorS[]	= { {"On", ISS_OFF}, {"Off", ISS_OFF}};
static ISwitch SlewAltAzS[]		= { {"Slew to Object Az/Alt", ISS_OFF}};

static ISwitches FanStatusSw		= { mydev, "Fan", FanStatusS, NARRAY(FanStatusS), ILS_IDLE, 0, LX16Group };
static ISwitches HomeSearchSw		= { mydev, "Park", HomeSearchS, NARRAY(HomeSearchS), ILS_IDLE, 0, LX16Group};
static ISwitches FieldDeRotatorSw	= { mydev, "Field De-rotator", FieldDeRotatorS, NARRAY(FieldDeRotatorS), ILS_IDLE, 0, LX16Group};
static ISwitches SlewAltAzSw		= { mydev, "Slew to Object Az/Alt", SlewAltAzS, NARRAY(SlewAltAzS), ILS_IDLE, 0, LX16Group};

static INumber ALT		        = { mydev, "Altitude D:M:S up", NULL, ILS_IDLE, 0, LX16Group};
static INumber AZ			= { mydev, "Azimuth D:M:S E of N", NULL, ILS_IDLE, 0, LX16Group};


LX200_16::LX200_16() : LX200Autostar()
{

 ALT.nstr	= strcpy( new char[9], "DD:MM");
 AZ.nstr	= strcpy( new char[9], "DDD:MM");

}

void LX200_16::ISGetProperties (const char *dev)
{

if (dev && strcmp (mydev, dev))
    return;

 // process parent first
  LX200Autostar::ISGetProperties(dev);

  ICDefNumber (&ALT, "ALT", IP_RW, NULL);
  ICDefNumber (&AZ , "AZ", IP_RW, NULL);

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
  float h,d,m,s;

  LX200Autostar::ISNewNumber(n);

  // ignore if not ours //
  if (strcmp (n->dev, mydev))
    return;

  if ( !strcmp (n->name, AZ.name) )
	{
	  if (checkPower())
	  {
	    AZ.s = ILS_IDLE;
	    ICSetNumber(&AZ, NULL);
	    return;
	  }

	  if (validateSex(n->nstr, &h, &m,&s))
	  {
            AZ.s = ILS_IDLE;
	    ICSetNumber(&AZ, NULL);
	    return;
	  }
	  if (m > 59 || m < 0 || h < 0 || h > 360)
	  {
 	    AZ.s = ILS_IDLE;
	    ICSetNumber(&AZ , "Coordinates invalid");
	    return;
	  }

	  getSex(n->nstr, &targetAz);
          setObjAz( (int) h, (int) m);
	  AZ.s = ILS_OK;
	  strcpy(AZ.nstr , n->nstr);
	  handleAltAzSlew();
	  return;
        }

	if ( !strcmp (n->name, ALT.name) )
	{
	  if (checkPower())
	  {
	    ALT.s = ILS_IDLE;
	    ICSetNumber(&ALT, NULL);
	    return;
	  }

	  if (validateSex(n->nstr, &d, &m, &s))
	  {
            ALT.s = ILS_IDLE;
	    ICSetNumber(&ALT , NULL);
	    return;
	  }
	  if (s > 60 || s < 0 || m > 59 || m < 0 || d < -90 || d > 90)
	  {
 	    ALT.s = ILS_IDLE;
	    ICSetNumber(&ALT , "Coordinates invalid");
	    return;
	  }

	  getSex(n->nstr, &targetAlt);
          setObjAlt( (int) h, (int) m);
	  ALT.s = ILS_OK;
	  strcpy(ALT.nstr , n->nstr);
	  handleAltAzSlew();
	  return;
        }

 }

void LX200_16::ISNewSwitch (ISwitches *s)
{
   int index[16];
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


}

void LX200_16::handleAltAzSlew()
{
        int i=0;

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

	  SlewAltAzSw.s = ILS_BUSY;
	  ICSetNumber(&ALT , NULL);
	  ICSetNumber(&AZ , NULL);
	  ICSetSwitch(&SlewAltAzSw, "Slewing to Az %s - Alt %s", AZ.nstr, ALT.nstr);
	  return;
}

 void LX200_16::ISPoll ()
 {
   int searchResult;
   double dx, dy;
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

	switch (SlewAltAzSw.s)
	{
	case ILS_IDLE:
	     break;

	case ILS_BUSY:

	    fprintf(stderr , "Getting LX200 RA, DEC...\n");
	    currentAz = getLX200Az();
	    currentAlt = getLX200Alt();
	    dx = targetAz - currentAz;
	    dy = targetAlt - currentAlt;

	    formatSex ( currentAz, AZ.nstr, XXYYZZ);
	    formatSex (currentAlt, ALT.nstr, SXXYYZZ);

	    if (dx < 0) dx *= -1;
	    if (dy < 0) dy *= -1;
	    fprintf(stderr, "targetAz is %f, currentAz is %f\n", (float) targetAz, (float) currentAz);
	    fprintf(stderr, "targetAlt is %f, currentAlt is %f\n****************************\n", (float) targetAlt, (float) currentAlt);


	    if (dx <= 0.001 && dy <= 0.001)
	    {

		SlewAltAzSw.s = ILS_OK;
		currentAz = targetAz;
		currentAlt = targetAlt;

		formatSex (targetAz, AZ.nstr, XXYYZZ);
		formatSex (targetAlt, ALT.nstr, SXXYYZZ);

		AZ.s = ILS_OK;
		ALT.s = ILS_OK;
		ICSetNumber (&AZ, NULL);
		ICSetNumber (&ALT, NULL);
		ICSetSwitch (&SlewAltAzSw, "Slew is complete");
	    } else
	    {
		ICSetNumber (&AZ, NULL);
		ICSetNumber (&ALT, NULL);
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

  formatSex ( (targetAz = getLX200Az()), AZ.nstr, XXXYY);
  formatSex ( (targetAlt = getLX200Alt()), ALT.nstr, SXXYY);

  ICSetNumber (&AZ, NULL);
  ICSetNumber (&ALT, NULL);

 }
