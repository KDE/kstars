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
#include "deepskyobject.h"
#include "ksplanetbase.h"
#include "skyobjitem.h"
#include "ksutils.h"

SkyObjItem::SkyObjItem(SkyObject *so) : m_Name(so->name()), m_LongName(so->longname()),m_TypeName(so->typeName()), m_So(so), skd(NULL)
{
    switch (so->type())
    {
    case SkyObject::PLANET:
        m_Type = Planet;
        break;
    case SkyObject::STAR:
        skd = new SkyObjDescription(m_Name, m_TypeName);
        m_Type = Star;
        break;
    case SkyObject::CONSTELLATION:
        skd = new SkyObjDescription(m_Name, m_TypeName);
        m_Type = Constellation;
        break;
    case SkyObject::GALAXY:
        skd = new SkyObjDescription(m_LongName, "");
        m_Type = Galaxy;
        break;
    case SkyObject::OPEN_CLUSTER:
    case SkyObject::GLOBULAR_CLUSTER:
    case SkyObject::GALAXY_CLUSTER:
        if(m_Name.contains("NGC", Qt::CaseInsensitive))
            skd = new SkyObjDescription(m_Name, "");
        m_Type = Cluster;
        break;
    case SkyObject::PLANETARY_NEBULA:
    case SkyObject::GASEOUS_NEBULA:
    case SkyObject::DARK_NEBULA:
        if(m_Name.contains("NGC", Qt::CaseInsensitive))
            skd = new SkyObjDescription(m_Name, "");
        m_Type = Nebula;
        break;
    }

    setPosition(m_So);
}

SkyObjItem::~SkyObjItem(){
    delete skd;
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
    KStarsData *data = KStarsData::Instance();
    KStarsDateTime ut = data->geo()->LTtoUT(KStarsDateTime(KDateTime::currentLocalDateTime()));
    SkyPoint sp = so->recomputeCoords(ut, data->geo());

    //check altitude of object at this time.
    sp.EquatorialToHorizontal(data->lst(), data->geo()->lat());
    double rounded_altitude = (int)(sp.alt().Degrees()/5.0)*5.0;

    m_Position = i18n("Now visible: About %1 degrees above the %2 horizon", rounded_altitude, KSUtils::toDirectionString( sp.az() ) );
}

QString SkyObjItem::getDesc() const
{
    if (m_Type == Planet)
    {
        KSFileReader fileReader;
        if (!fileReader.open("PlanetFacts.dat"))
            return i18n("No Description found for selected sky-object");

        while (fileReader.hasMoreLines())
        {
            QString line = fileReader.readLine();
            if(line.length() != 0 && line[0] != '#')
            {
                QString soname = line.split("::")[0];
                QString desc = line.split("::")[1];
                if (soname == m_Name)
                {
                    return desc;
                }
            }
        }
    }

    if(skd)
    {
        if(skd->downloadedData() != "")
            return skd->downloadedData();
    }

    if(m_Type == Star)
        return i18n("Bright Star");

    return getTypeName();

}

QString SkyObjItem::getDescSource()
{
    if (m_Type == Planet)
    {
        return i18n("(Source: Wikipedia)");
    }

    if(skd)
    {
        if(skd->downloadedData() != "")
            return i18n("(Source: Wikipedia)");
    }

    return i18n("(Source: N/A)");
}


QString SkyObjItem::getSurfaceBrightness() const
{
    /** Surface Brightness is applicable only for extended light sources like
      * Deep-Sky Objects. Here we use the formula SB = m + log10(a*b/4)
      * where m is the magnitude of the sky-object. a and b are the major and minor
      * axis lengths of the objects respectively in arcminutes. SB is the surface
      * brightness obtained in mag * arcminutes^-2
      */

    DeepSkyObject *dso = (DeepSkyObject *)m_So;
    float SB = m_So->mag() + 2.5 * log10(dso->a() * dso->b() / 4);

    switch(getType())
    {
    case Galaxy:
    case Nebula:
        return KGlobal::locale()->formatNumber(SB, 2) + " mag/arcmin^2";
    default:
        return QString(" --"); // Not applicable for other sky-objects
    }
}

QString SkyObjItem::getSize() const
{
    switch (getType())
    {
    case Galaxy:
    case Cluster:
    case Nebula:
        return KGlobal::locale()->formatNumber(((DeepSkyObject *)m_So)->a(), 2) + " arcminutes";
    case Planet:
        return KGlobal::locale()->formatNumber(((KSPlanetBase *)m_So)->angSize(), 2) + " arcseconds";
    default:
        return QString(" --");
    }
}
