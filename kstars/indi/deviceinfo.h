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
        DeviceInfo(DriverInfo *parent, const std::shared_ptr<INDI::BaseDevice> &dp);

        const QString  &getDeviceName() const
        {
            return m_Name;
        }
        DriverInfo *getDriverInfo()
        {
            return m_Parent;
        }
        const std::shared_ptr<INDI::BaseDevice> &getBaseDevice()
        {
            return m_BaseDevice;
        }

    private:
        DriverInfo *m_Parent { nullptr};
        std::shared_ptr<INDI::BaseDevice> m_BaseDevice;
        QString m_Name;
};

#endif // DEVICEINFO_H
