/*
    LX200 16"
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

#include "lx200_16.h"
#include "lx200driver.h"


extern LX200Generic *telescope;
extern char mydev[];

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
static ISwitch HomeSearchS[]		= { {"Seek home and save", ISS_OFF} , {"Seek home and set"}};
static ISwitch FieldDeRotatorS[]	= { {"On", ISS_OFF}, {"Off", ISS_OFF}};

static ISwitches FanStatusSw		= { mydev, "Fan", FanStatusS, NARRAY(FanStatusS), ILS_IDLE, 0};
static ISwitches HomeSearchSw		= { mydev, "Home Search", HomeSearchS, NARRAY(HomeSearchS), ILS_IDLE, 0};
static ISwitches FieldDeRotatorSw	= { mydev, "Field De-rotator", FieldDeRotatorS, NARRAY(FieldDeRotatorS), ILS_IDLE, 0};

static INumber ObjectAlt		= { mydev, "Object Altitude", NULL, ILS_IDLE};
static INumber ObjectAz			= { mydev, "Object Azimuth", NULL, ILS_IDLE};



 
#ifdef LX200_SIXTEEN
void
ISInit()
{
	fprintf(stderr , "initilizaing from LX200 16 device...\n");

	// Two important steps always performed when adding a sub-device
	// 1. mydev = device_name
	strcpy(mydev, "LX200 16");
	// 2. device = sub_class
	telescope = new LX200_16();

}

#endif

LX200_16::LX200_16() : LX200Autostar()
{



}

void LX200_16::ISGetProperties (char *dev)
{

if (dev && strcmp (mydev, dev))
    return;

 // process parent first
 if (callParent)
    LX200Autostar::ISGetProperties(dev);
 callParent = 1;

}

void LX200_16::ISNewText (IText *t)
{

  if (callParent)
     LX200Autostar::ISNewText(t);
  callParent = 1;

}

void LX200_16::ISNewNumber (INumber *n)
{
  if (callParent)
     LX200Autostar::ISNewNumber(n);
  callParent = 1;

 }

 void LX200_16::ISNewSwitch (ISwitches *s)
 {

   if (callParent)
      LX200Autostar::ISNewSwitch(s);
   callParent = 1;



 }

 void LX200_16::ISPoll ()
 {

   if (callParent)
      LX200Autostar::ISPoll();
   callParent = 1;

 }

 void LX200_16::getBasicData()
 {
   // process parent first
   if (callParent)
      LX200Autostar::getBasicData();
   callParent = 1;

 }
