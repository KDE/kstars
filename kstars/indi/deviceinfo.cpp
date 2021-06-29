/*  INDI Device Info
    Copyright (C) 2012 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

 */

#include "deviceinfo.h"

DeviceInfo::DeviceInfo(DriverInfo *parent, INDI::BaseDevice *ibd)
{
    drv = parent;
    dp  = ibd;
    m_Name = ibd->getDeviceName();
}
