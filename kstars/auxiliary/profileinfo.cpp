/*  Profile Info
    Copyright (C) 2016 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

 */

#include "profileinfo.h"


ProfileInfo::ProfileInfo(int id, const QString &name)
{    
    this->name =  name;
    this->id   = id;
    port = INDIWebManagerPort = -1;
}

QString ProfileInfo::mount()
{
    if (drivers.contains("Mount"))
        return drivers["Mount"];
    else
        return QString();
}

QString ProfileInfo::ccd()
{
    if (drivers.contains("CCD"))
        return drivers["CCD"];
    else
        return QString();
}

QString ProfileInfo::guider()
{
    if (drivers.contains("Guider"))
        return drivers["Guider"];
    else
        return QString();
}

QString ProfileInfo::focuser()
{
    if (drivers.contains("Focuser"))
        return drivers["Focuser"];
    else
        return QString();
}

QString ProfileInfo::filter()
{
    if (drivers.contains("Filter"))
        return drivers["Filter"];
    else
        return QString();
}

QString ProfileInfo::dome()
{
    if (drivers.contains("Dome"))
        return drivers["Dome"];
    else
        return QString();
}

QString ProfileInfo::weather()
{
    if (drivers.contains("Weather"))
        return drivers["Weather"];
    else
        return QString();
}

QString ProfileInfo::ao()
{
    if (drivers.contains("AO"))
        return drivers["AO"];
    else
        return QString();
}

QString ProfileInfo::aux1()
{
    if (drivers.contains("Aux1"))
        return drivers["Aux1"];
    else
        return QString();
}

QString ProfileInfo::aux2()
{
    if (drivers.contains("Aux2"))
        return drivers["Aux2"];
    else
        return QString();
}

QString ProfileInfo::aux3()
{
    if (drivers.contains("Aux3"))
        return drivers["Aux3"];
    else
        return QString();
}

QString ProfileInfo::aux4()
{
    if (drivers.contains("Aux4"))
        return drivers["Aux4"];
    else
        return QString();
}
