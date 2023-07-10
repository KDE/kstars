/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
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
        DeviceInfo(const QSharedPointer<DriverInfo> &, INDI::BaseDevice ibd);

        const QString  &getDeviceName() const
        {
            return m_Name;
        }
        const QSharedPointer<DriverInfo> &getDriverInfo()
        {
            return m_Driver;
        }
        INDI::BaseDevice getBaseDevice()
        {
            return dp;
        }

    private:
        QSharedPointer<DriverInfo> m_Driver;
        INDI::BaseDevice dp;
        QString m_Name;
};

#endif // DEVICEINFO_H
