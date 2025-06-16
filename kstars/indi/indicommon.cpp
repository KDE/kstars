/*
    SPDX-FileCopyrightText: 2024 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "indicommon.h"

DeviceFamily toDeviceFamily(const QString &family)
{
    if (family == "Mount")
        return KSTARS_TELESCOPE;
    if (family == "CCD")
        return KSTARS_CCD;
    if (family == "Guider")
        return KSTARS_CCD;
    if (family == "Focuser")
        return KSTARS_FOCUSER;
    if (family == "Filter")
        return KSTARS_FILTER;
    if (family == "Dome")
        return KSTARS_DOME;
    if (family == "Weather")
        return KSTARS_WEATHER;
    if (family == "AO")
        return KSTARS_ADAPTIVE_OPTICS;
    if (family == "Power")
        return KSTARS_POWER;
    if (family == "Aux1" || family == "Aux2" || family == "Aux3" || family == "Aux4")
        return KSTARS_AUXILIARY;

    return DeviceFamilyLabels.key(family, KSTARS_UNKNOWN);
}

QString fromDeviceFamily(DeviceFamily family)
{
    return DeviceFamilyLabels.value(family, "Unknown");
}
