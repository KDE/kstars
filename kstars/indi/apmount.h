/*
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

*/

#ifndef ASTROPHYSICS_H
#define ASTROPHYSICS_H

#include "indidevapi.h"
#include "indicom.h"

#define	POLLMS		1000				/* poll period, ms */
#define     mydev		"Astro-Physics"		/* The device name */



/* equatorial position */

// N.B. No Static identifier as it is needed for external linkage



class APMount
{
 public:
 APMount();
 ~APMount();

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
  ISwitch AlignmentS [2];
  ISwitch PowerS[2];
  ISwitch OnCoordSetS[3];
  ISwitch TrackModeS[4];
  ISwitch AbortSlewS[1];
  ISwitch ParkS[2];
  ISwitch MovementS[4];
  ISwitch FocusMotionS[2];

  /* Texts */
  IText PortT[1];
  IText UTCT[1];
  IText ObjectT[1];

  /* Numbers */
  INumber EqN[2];
  INumber GeoN[2];
  INumber FocusTimerN[1];
  INumber SDTimeN[1];
  INumber HorN[2];
  INumber FocusSpeedN[1];

  /* Switch Vectors */
  ISwitchVectorProperty PowerSP;
  ISwitchVectorProperty AlignmentSP;
  ISwitchVectorProperty OnCoordSetSP;
  ISwitchVectorProperty AbortSlewSP;
  ISwitchVectorProperty ParkSP;
  ISwitchVectorProperty TrackModeSP;
  ISwitchVectorProperty MovementSP;
  ISwitchVectorProperty FocusMotionSP;

   /* Number Vectors */
  INumberVectorProperty EqNP;
  INumberVectorProperty GeoNP;
  INumberVectorProperty FocusTimerNP;
  INumberVectorProperty SDTimeNP;
  INumberVectorProperty HorNP;
  INumberVectorProperty FocusSpeedNP;

   /* Text Vectors */
   ITextVectorProperty PortTP;
   ITextVectorProperty TimeTP;
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
 void getAlignment();
 int handleCoordSet();
 int getOnSwitch(ISwitchVectorProperty *sp);
 void correctFault();
 void enableSimulation(bool enable);
 void updateTime();
 void updateLocation();
 

 protected:
  int timeFormat;
  int currentSiteNum;
  int trackingMode;

  double JD;
  double targetRA;
  double targetDEC;
  double lastRA;
  double lastDEC;
  double UTCOffset;
  bool   fault;
  bool   simulation;

  struct tm *localTM;
  
  int currentSet;
  int lastSet;
  int lastMove[4];

};

#endif
