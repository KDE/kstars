/*
    LX200 Generic
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

#ifndef LX200GENERIC_H
#define LX200GENERIC_H

#include "indiapi.h"
#include "indicom.h"

#define	POLLMS		2000		/* poll period, ms */

class LX200Generic
{
 public:
 LX200Generic();
 virtual ~LX200Generic() {}

 virtual void ISGetProperties (const char *dev);
 virtual void ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
 virtual void ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);
 virtual void ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
 virtual void ISPoll ();
 virtual void getBasicData();

 int checkPower(INumberVectorProperty *np);
 int checkPower(ISwitchVectorProperty *sp);
 int checkPower(ITextVectorProperty *tp);
 bool isTelescopeOn(void);
 void powerTelescope(ISState *s);
 void slewError(int slewCode);
 void getAlignment();
 int handleCoordSet();
 int getOnSwitch(ISState * states, int n);
 void resetSwitches(ISwitchVectorProperty *driverSw);

 protected:
  int timeFormat;
  int currentSiteNum;
  int currentCatalog;
  int currentSubCatalog;
  int trackingMode;

  double JD;
  double currentRA;
  double currentDEC;
  double targetRA;
  double targetDEC;
  double lastRA;
  double lastDEC;

  struct tm *localTM;

  int lastSet;
  int lastMove[4];

};



#endif
