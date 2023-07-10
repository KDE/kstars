/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "deviceinfo.h"

DeviceInfo::DeviceInfo(const QSharedPointer<DriverInfo> &parent, INDI::BaseDevice ibd)
{
    m_Driver = parent;
    dp  = ibd;
    m_Name = ibd.getDeviceName();
}
