/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "deviceinfo.h"

DeviceInfo::DeviceInfo(DriverInfo *parent, const std::shared_ptr<INDI::BaseDevice> &dp) :
    m_Parent(parent), m_BaseDevice(dp)
{
    m_Name = dp->getDeviceName();
}
