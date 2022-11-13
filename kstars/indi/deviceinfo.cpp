/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "deviceinfo.h"

DeviceInfo::DeviceInfo(DriverInfo *parent, INDI::BaseDevice *ibd)
{
    drv = parent;
    dp  = ibd;
    m_Name = ibd->getDeviceName();
}
