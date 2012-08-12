/***************************************************************************
                          skyobjitem.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 2012/21/06
    copyright            : (C) 2012 by Samikshan Bairagya
    email                : samikshan@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "ksfilereader.h"
#include "kstarsdata.h"
#include "skyobjitem.h"

SkyObjItem::SkyObjItem(SkyObject *so) : m_Name(so->name()), m_LongName(so->longname()),m_TypeName(so->typeName()), m_So(so)
{
    switch (so->type())
    {
    case SkyObject::PLANET:
        m_Type = Planet;
        break;
    case SkyObject::STAR:
        m_Type = Star;
        break;
    case SkyObject::CONSTELLATION:
        m_Type = Constellation;
        break;
    case SkyObject::GALAXY:
        m_Type = Galaxy;
        break;
    case SkyObject::OPEN_CLUSTER:
    case SkyObject::GLOBULAR_CLUSTER:
    case SkyObject::GALAXY_CLUSTER:
        m_Type = Cluster;
        break;
    case SkyObject::PLANETARY_NEBULA:
    case SkyObject::GASEOUS_NEBULA:
    case SkyObject::DARK_NEBULA:
        m_Type = Nebula;
        break;
    }

    setPosition(m_So);
}

QVariant SkyObjItem::data(int role)
{
    switch(role)
    {
        case DispNameRole:
            return getLongName();
        case CategoryRole:
            return getType();
        case CategoryNameRole:
            return getTypeName();
        default:
            return QVariant();
    }
}

QHash<int, QByteArray> SkyObjItem::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[DispNameRole] = "dispName";
    roles[CategoryRole] = "type";
    roles[CategoryNameRole] = "typeName";
    return roles;
}

void SkyObjItem::setPosition(SkyObject* so)
{
    const QString cardinals[] = {
        "N", "NNE", "NE", "ENE",
        "E", "ESE", "SE", "SSE",
        "S", "SSW", "SW", "WSW",
        "W", "WNW", "NW", "NNW"
    } ;
    KStarsData *data = KStarsData::Instance();
    KStarsDateTime ut = data->geo()->LTtoUT(KStarsDateTime(KDateTime::currentLocalDateTime()));
    SkyPoint sp = so->recomputeCoords(ut, data->geo());

    //check altitude of object at this time.
    sp.EquatorialToHorizontal(data->lst(), data->geo()->lat());
    double rounded_altitude = (int)(sp.alt().Degrees()/5.0)*5.0;
    int rounded_azimuth = (int)(sp.az().Degrees()/22.5);

    m_Position = QString("Now visible: About ") + (QString::number(rounded_altitude)) + (" degrees above the ") + (cardinals[rounded_azimuth]) + (" horizon ");
}

QString SkyObjItem::getDesc() const
{
    if (m_Type == Planet)
    {
        KSFileReader fileReader;
        if (!fileReader.open("PlanetFacts.dat"))
            return QString("No Description found for selected sky-object");

        while (fileReader.hasMoreLines())
        {
            QString line = fileReader.readLine();
            if (line.split("::")[0] == m_Name)
                return line.split("::")[1];
        }
    }
    else if (m_Type == Star)
    {
        return "Bright Star";
    }
    else if (m_Type == Constellation)
    {
        return "Constellation";
    }

    return QString("No Description found for selected sky-object");
}

QString SkyObjItem::getMagnitude() const
{
    return QString("Magnitude : ") + (QString::number(m_So->mag()));
}
