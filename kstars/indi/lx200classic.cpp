/*
    LX200 Classoc
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

#include "lx200classic.h"
#include "lx200driver.h"

extern LX200Generic *telescope;
extern INumberVectorProperty eqNum;
extern int MaxReticleFlashRate;
extern char mydev[];

#define BASIC_GROUP	"Basic Data"
#define LIBRARY_GROUP	"Library"

static IText   ObjectText[] = {{"objectText", "Info", ""}};
static ITextVectorProperty ObjectInfo = {mydev, "Object Info", "", BASIC_GROUP, IP_RO, 0, IPS_IDLE, ObjectText, NARRAY(ObjectText)};

/* Library group */
static ISwitch StarCatalogS[]    = {{"STAR", "", ISS_ON}, {"SAO", "", ISS_OFF}, {"GCVS", "", ISS_OFF}};
static ISwitch DeepSkyCatalogS[] = {{"NGC", "", ISS_ON}, {"IC", "", ISS_OFF}, {"UGC", "", ISS_OFF}, {"Caldwell", "", ISS_OFF}, {"Arp", "", ISS_OFF}, {"Abell", "", ISS_OFF}, {"Messier", "", ISS_OFF}};
static ISwitch SolarS[]          = { {"Select", "Select item...", ISS_ON}, {"1", "Mercury", ISS_OFF}, {"2", "Venus", ISS_OFF}, {"3", "Moon", ISS_OFF}, {"4", "Mars", ISS_OFF}, {"5", "Jupiter", ISS_OFF}, {"6", "Saturn", ISS_OFF}, {"7", "Uranus", ISS_OFF}, {"8", "Neptune", ISS_OFF}, {"9", "Pluto", ISS_OFF}};

static ISwitchVectorProperty StarCatalogSw   = { mydev, "Star Catalogs", "", LIBRARY_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, StarCatalogS, NARRAY(StarCatalogS)};
static ISwitchVectorProperty DeepSkyCatalogSw= { mydev, "Deep Sky Catalogs", "", LIBRARY_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, DeepSkyCatalogS, NARRAY(DeepSkyCatalogS)};
static ISwitchVectorProperty SolarSw         = { mydev, "SOLAR_SYSTEM", "Solar System", LIBRARY_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, SolarS, NARRAY(SolarS)};

static INumber ObjectN[] = { "ObjectN", "Number", "%g", 1., 10000., 1., 0.};
static INumberVectorProperty ObjectNo= { mydev, "Object Number", "", LIBRARY_GROUP, IP_RW, 0, IPS_IDLE, ObjectN, NARRAY(ObjectN) };

LX200Classic::LX200Classic() : LX200Generic()
{
   ObjectInfo.t[0].text = new char[128];
   strcpy(ObjectInfo.t[0].text, ""); 

}


void LX200Classic::ISGetProperties (const char *dev)
{

if (dev && strcmp (mydev, dev))
    return;

  LX200Generic::ISGetProperties(dev);

  IDDefText   (&ObjectInfo);
  IDDefSwitch (&SolarSw);
  IDDefSwitch (&StarCatalogSw);
  IDDefSwitch (&DeepSkyCatalogSw);
  IDDefNumber (&ObjectNo);

}

void LX200Classic::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
  LX200Generic::ISNewText (dev, name, texts, names, n);

}


void LX200Classic::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    if ( !strcmp (name, ObjectNo.name) )
	{
	  if (checkPower(&ObjectNo))
	    return;

	  selectCatalogObject( currentCatalog, (int) values[0]);
          getObjectInfo(ObjectText[0].text);
	  eqNum.n[0].value = getLX200RA();
	  eqNum.n[1].value = getLX200DEC();

	  ObjectNo.s = eqNum.s = IPS_OK;
	  IDSetNumber(&ObjectNo , "Object updated");
	  IDSetNumber(&eqNum, NULL);
	  IDSetText  (&ObjectInfo, NULL);

	  handleCoordSet();

	  return;
        }


    LX200Generic::ISNewNumber (dev, name, values, names, n);
}

 void LX200Classic::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
 {

      int index=0;

        // Star Catalog
	if (!strcmp (name, StarCatalogSw.name))
	{
	  if (checkPower(&StarCatalogSw))
	   return;

	 index = getOnSwitch(states, n);
	 resetSwitches(&StarCatalogSw);
	 StarCatalogSw.sw[index].s = ISS_ON;

	 currentCatalog = LX200_STAR_C;

	  if (selectSubCatalog(currentCatalog, index))
	  {
	   currentSubCatalog = index;
	   StarCatalogSw.s = IPS_OK;
	   IDSetSwitch(&StarCatalogSw, NULL);
	  }
	  else
	  {
	   StarCatalogSw.s = IPS_IDLE;
	   IDSetSwitch(&StarCatalogSw, "Catalog unavailable");
	  }
	  return;
	}

	// Deep sky catalog
	if (!strcmp (name, DeepSkyCatalogSw.name))
	{
	  if (checkPower(&DeepSkyCatalogSw))
	   return;

	index = getOnSwitch(states, n);
	resetSwitches(&DeepSkyCatalogSw);
	DeepSkyCatalogSw.sw[index].s = ISS_ON;

	  if (index == LX200_MESSIER_C)
	  {
	    currentCatalog = index;
	    DeepSkyCatalogSw.s = IPS_OK;
	    IDSetSwitch(&DeepSkyCatalogSw, NULL);
	    return;
	  }
	  else
	    currentCatalog = LX200_DEEPSKY_C;

	  if (selectSubCatalog(currentCatalog, index))
	  {
	   currentSubCatalog = index;
	   DeepSkyCatalogSw.s = IPS_OK;
	   IDSetSwitch(&DeepSkyCatalogSw, NULL);
	  }
	  else
	  {
	   DeepSkyCatalogSw.s = IPS_IDLE;
	   IDSetSwitch(&DeepSkyCatalogSw, "Catalog unavailable");
	  }
	  return;
	}

	if (!strcmp (name, SolarSw.name))
	{

	  if (checkPower(&SolarSw))
	   return;

	index = getOnSwitch(states, n);

	  // We ignore the first option : "Select item"
	  if (index == 0)
	  {
	    SolarSw.s  = IPS_IDLE;
	    IDSetSwitch(&SolarSw, NULL);
	    return;
	  }

          resetSwitches(&SolarSw);
	  SolarSw.sw[index].s = ISS_ON;

          selectSubCatalog ( LX200_STAR_C, LX200_STAR);
	  selectCatalogObject( LX200_STAR_C, index + 900);

	  ObjectNo.s = IPS_OK;
	  SolarSw.s  = IPS_OK;

	  getObjectInfo(ObjectInfo.t[0].text);
	  IDSetNumber(&ObjectNo , "Object updated");
	  IDSetSwitch(&SolarSw, NULL);

	  if (currentCatalog == LX200_STAR_C || currentCatalog == LX200_DEEPSKY_C)
	  	selectSubCatalog( currentCatalog, currentSubCatalog);

	  targetRA = getObjectRA();
	  targetDEC = getObjectDEC();

	  handleCoordSet();

	  return;
	}

   LX200Generic::ISNewSwitch (dev, name, states, names,  n);

 }

 void LX200Classic::ISPoll ()
 {

      LX200Generic::ISPoll();

 }

 void LX200Classic::getBasicData()
 {

   // process parent first
   LX200Generic::getBasicData();

 }
