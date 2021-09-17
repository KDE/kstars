/*
    SPDX-FileCopyrightText: 2016 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "profileinfo.h"

ProfileInfo::ProfileInfo(int id, const QString &name)
{
    this->name = name;
    this->id   = id;
    port = INDIWebManagerPort = guiderport = -1;
}

QString ProfileInfo::mount() const
{
    if (drivers.contains("Mount"))
        return drivers["Mount"];
    else
        return QString();
}

QString ProfileInfo::ccd() const
{
    if (drivers.contains("CCD"))
        return drivers["CCD"];
    else
        return QString();
}

QString ProfileInfo::guider() const
{
    if (drivers.contains("Guider"))
        return drivers["Guider"];
    else
        return QString();
}

QString ProfileInfo::focuser() const
{
    if (drivers.contains("Focuser"))
        return drivers["Focuser"];
    else
        return QString();
}

QString ProfileInfo::filter() const
{
    if (drivers.contains("Filter"))
        return drivers["Filter"];
    else
        return QString();
}

QString ProfileInfo::dome() const
{
    if (drivers.contains("Dome"))
        return drivers["Dome"];
    else
        return QString();
}

QString ProfileInfo::weather() const
{
    if (drivers.contains("Weather"))
        return drivers["Weather"];
    else
        return QString();
}

QString ProfileInfo::ao() const
{
    if (drivers.contains("AO"))
        return drivers["AO"];
    else
        return QString();
}

QString ProfileInfo::aux1() const
{
    if (drivers.contains("Aux1"))
        return drivers["Aux1"];
    else
        return QString();
}

QString ProfileInfo::aux2() const
{
    if (drivers.contains("Aux2"))
        return drivers["Aux2"];
    else
        return QString();
}

QString ProfileInfo::aux3() const
{
    if (drivers.contains("Aux3"))
        return drivers["Aux3"];
    else
        return QString();
}

QString ProfileInfo::aux4() const
{
    if (drivers.contains("Aux4"))
        return drivers["Aux4"];
    else
        return QString();
}

QJsonObject ProfileInfo::toJson() const
{
    return
    {
        {"name", name},
        {"auto_connect", autoConnect},
        {"port_selector", portSelector},
        {"mode", host.isEmpty() ? "local" : "remote"},
        {"remote_host", host},
        {"remote_port", port},
        {"indihub", indihub},
        {"guiding", guidertype},
        {"remote_guiding_host", guiderhost},
        {"remote_guiding_port", guiderport},
        {"use_web_manager", INDIWebManagerPort != -1},
        {"primary_scope", primaryscope},
        {"guide_scope", guidescope},
        {"mount", mount()},
        {"ccd", ccd()},
        {"guider", guider()},
        {"focuser", focuser()},
        {"filter", filter()},
        {"ao", ao()},
        {"dome", dome()},
        {"weather", weather()},
        {"aux1", aux1()},
        {"aux2", aux2()},
        {"aux3", aux3()},
        {"aux4", aux4()},
        {"remote", remotedrivers},
    };
}
