/*
    LX200 Basic Driver
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

*/

#ifndef LX200BASIC_H
#define LX200BASIC_H

#include "indidevapi.h"
#include "indicom.h"

#define	POLLMS		1000			/* poll period, ms */
#define mydev		"LX200 Basic"		/* The device name */



class LX200Basic
{
 public:
 LX200Basic();
 ~LX200Basic();

 void ISGetProperties (const char *dev);
 void ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
 void ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);
 void ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
 void ISPoll ();
 
  void connectionLost();
  void connectionResumed();

private:

  void initProperties();

  /* Switches */
  ISwitch PowerS[2];
  ISwitch OnCoordSetS[3];
  ISwitch AbortSlewS[1];
  
  /* Texts */
  IText PortT[1];
  IText ObjectT[1];

  /* Numbers */
  INumber EqN[2];
  
  /* Switch Vectors */
  ISwitchVectorProperty PowerSP;
  ISwitchVectorProperty OnCoordSetSP;
  ISwitchVectorProperty AbortSlewSP;
  
   /* Number Vectors */
  INumberVectorProperty EqNP;
  
   /* Text Vectors */
   ITextVectorProperty PortTP;
   ITextVectorProperty ObjectTP;

 void getBasicData();
 int checkPower(INumberVectorProperty *np);
 int checkPower(ISwitchVectorProperty *sp);
 int checkPower(ITextVectorProperty *tp);
 void handleError(ISwitchVectorProperty *svp, int err, const char *msg);
 void handleError(INumberVectorProperty *nvp, int err, const char *msg);
 void handleError(ITextVectorProperty *tvp, int err, const char *msg);
 bool isTelescopeOn(void);
 void powerTelescope();
 void slewError(int slewCode);
  int handleCoordSet();
 int getOnSwitch(ISwitchVectorProperty *sp);
 void correctFault();
 void enableSimulation(bool enable);
 

 protected:

  double JD;
  double targetRA;
  double targetDEC;
  double lastRA;
  double lastDEC;
  double UTCOffset;
  bool   simulation;
  bool   fault;

  struct tm *localTM;
  
  int currentSet;
  int lastSet;

};

#endif
