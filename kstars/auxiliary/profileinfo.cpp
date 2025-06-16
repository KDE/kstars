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

void ProfileInfo::addDriver(DeviceFamily family, const QString &driver)
{
    if (drivers.contains(family))
    {
        drivers[family].append(driver);
    }
    else
    {
        QList<QString> driverList;
        driverList.append(driver);
        drivers.insert(family, driverList);
    }
}

QList<QString> ProfileInfo::getDrivers(DeviceFamily family) const
{
    if (drivers.contains(family))
        return drivers[family];
    else
        return QList<QString>();
}

QString ProfileInfo::mount() const
{
    auto driverList = getDrivers(KSTARS_TELESCOPE);
    if (driverList.isEmpty())
        return QString();
    return driverList.first();
}

QString ProfileInfo::ccd() const
{
    auto driverList = getDrivers(KSTARS_CCD);
    if (driverList.isEmpty())
        return QString();
    return driverList.first();
}

QString ProfileInfo::guider() const
{
    auto driverList = getDrivers(KSTARS_CCD);
    if (driverList.count() > 1)
        return driverList.at(1);

    return QString();
}

QString ProfileInfo::focuser() const
{
    auto driverList = getDrivers(KSTARS_FOCUSER);
    if (driverList.isEmpty())
        return QString();
    return driverList.first();
}

QString ProfileInfo::filter() const
{
    auto driverList = getDrivers(KSTARS_FILTER);
    if (driverList.isEmpty())
        return QString();
    return driverList.first();
}

QString ProfileInfo::dome() const
{
    auto driverList = getDrivers(KSTARS_DOME);
    if (driverList.isEmpty())
        return QString();
    return driverList.first();
}

QString ProfileInfo::weather() const
{
    auto driverList = getDrivers(KSTARS_WEATHER);
    if (driverList.isEmpty())
        return QString();
    return driverList.first();
}

QString ProfileInfo::ao() const
{
    auto driverList = getDrivers(KSTARS_ADAPTIVE_OPTICS);
    if (driverList.isEmpty())
        return QString();
    return driverList.first();
}

QString ProfileInfo::aux1() const
{
    auto driverList = getDrivers(KSTARS_AUXILIARY);
    if (driverList.isEmpty())
        return QString();
    return driverList.at(0);
}

QString ProfileInfo::aux2() const
{
    auto driverList = getDrivers(KSTARS_AUXILIARY);
    if (driverList.count() < 2)
        return QString();
    return driverList.at(1);
}

QString ProfileInfo::aux3() const
{
    auto driverList = getDrivers(KSTARS_AUXILIARY);
    if (driverList.count() < 3)
        return QString();
    return driverList.at(2);
}

QString ProfileInfo::aux4() const
{
    auto driverList = getDrivers(KSTARS_AUXILIARY);
    if (driverList.count() < 4)
        return QString();
    return driverList.at(3);
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
        {"guiding", guidertype},
        {"remote_guiding_host", guiderhost},
        {"remote_guiding_port", guiderport},
        {"use_web_manager", INDIWebManagerPort != -1},
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
