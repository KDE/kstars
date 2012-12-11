#ifndef DEVICEINFO_H
#define DEVICEINFO_H

/*  INDI Device Info
    Copyright (C) 2012 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

 */

#include <basedevice.h>

#include "driverinfo.h"

class DeviceInfo
{
public:
    DeviceInfo(DriverInfo *parent, INDI::BaseDevice *ibd);

    DriverInfo *getDriverInfo() { return drv;}
    INDI::BaseDevice *getBaseDevice() { return dp;}

private:
    DriverInfo *drv;
    INDI::BaseDevice *dp;

};

#endif // DEVICEINFO_H
