#if 0
    LX200 Generic
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

#ifndef LX200GENERIC_H
#define LX200GENERIC_H

#include "indiapi.h"

#define	POLLMS		500		/* poll period, ms */

class LX200Generic
{
 public:
 LX200Generic();
 ~LX200Generic() {}

 virtual void ISGetProperties (char *dev);
 virtual void ISNewText (IText *t);
 virtual void ISNewNumber (INumber *n);
 virtual void ISNewSwitch (ISwitches *s);
 virtual void ISPoll ();
 virtual void getBasicData();

 int checkPower();
 int validateSwitch(ISwitches *clientSw, ISwitches *driverSw, int driverArraySize, int index[], int validatePower);
 void powerTelescope(ISwitches* s);
 void cleanUP();
 void slewError(int slewCode);
 void getAlignment();
 void alterSwitches(int OnOff, ISwitches * s, int switchArraySize);

 private:
  int timeFormat;
  int currentSiteNum;
  int currentCatalog;
  int currentSubCatalog;
  int portType;
  int portIndex;
  int trackingMode;

  double objectRA;
  double objectDEC;
  double targetRA;
  double targetDEC;
  double oldLX200RA;
  double oldLX200DEC;

protected:
  int callParent;


};



#endif
