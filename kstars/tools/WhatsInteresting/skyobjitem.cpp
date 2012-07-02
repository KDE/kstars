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

SkyObjItem::SkyObjItem(SkyObject* sobj, QObject* parent) : QObject(parent)
{
    so = sobj;
    name = sobj->name();
    type = sobj->typeName();
    setPosition(so);
}

QVariant SkyObjItem::data(int role)
{
    switch(role)
    {
        case DispNameRole:
            return getName();
        case CategoryRole:
            return getType();
        default:
            return QVariant();
    }
}

QHash< int, QByteArray > SkyObjItem::roleNames() const
{
    QHash<int, QByteArray > roles;
    roles[DispNameRole]="dispName";
    roles[CategoryRole] = "type";
    return roles;
}

void SkyObjItem::setPosition(SkyObject* so)
{
    QString cardinals[] = {
        "N", "NNE", "NE", "ENE",
        "E", "ESE", "SE", "SSE",
        "S", "SSW", "SW", "WSW",
        "W", "WNW", "NW", "NNW" 
    } ;
    KStarsData *data = KStarsData::Instance();
    KStarsDateTime ut = data->geo()->LTtoUT( KStarsDateTime(KDateTime::currentLocalDateTime()) );
    SkyPoint sp = so->recomputeCoords( ut, data->geo() );

    //check altitude of object at this time.
    sp.EquatorialToHorizontal( data->lst(), data->geo()->lat() );
    double rounded_altitude = (int)(sp.alt().Degrees()/5.0)*5.0;
    int rounded_azimuth = (int)(sp.az().Degrees()/22.5);

    position = QString("Now visible: ").append(QString::number(rounded_altitude)).append(" degrees above the ").append(cardinals[rounded_azimuth]).append(" horizon ");
}

QString SkyObjItem::getDesc()
{
    if (type == "Planet"){
        KSFileReader fileReader;
        if ( !fileReader.open("PlanetFacts.dat") )
            return QString("No Description found for selected sky-object");

        while ( fileReader.hasMoreLines() )
        {
            QString line = fileReader.readLine();
            if (line.split("::")[0] == name)
                return line.split("::")[1];
        }
    }
    else if (type == "Star")
    {
        return "Bright Star";
    }
    else if (type == "Constellation")
    {
        return "Constellation";
    }

    return QString("No Description found for selected sky-object");
}

QString SkyObjItem::getMagnitude()
{
    QString magtext = "Magnitude : ";
    magtext.append(QString::number(so->mag()));
    return magtext;
}
