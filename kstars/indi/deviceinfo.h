/*  INDI Device Info
    Copyright (C) 2012 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

*/

#ifndef DEVICEINFO_H
#define DEVICEINFO_H

#include <basedevice.h>

#include "driverinfo.h"

/**
 * @class DeviceInfo
 * DeviceInfo is simple class to hold DriverInfo and INDI::BaseDevice associated with a particular device.
 *
 * @author Jasem Mutlaq
 */
class DeviceInfo
{
    public:
        DeviceInfo(DriverInfo *parent, INDI::BaseDevice *ibd);

        const QString  &getDeviceName() const
        {
            return m_Name;
        }
        DriverInfo *getDriverInfo()
        {
            return drv;
        }
        INDI::BaseDevice *getBaseDevice()
        {
            return dp;
        }

    private:
        DriverInfo *drv;
        INDI::BaseDevice *dp;
        QString m_Name;
};

#endif // DEVICEINFO_H
