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
#include "kspaths.h"

SkyObjItem::SkyObjItem(SkyObject * so) : m_Name(so->name()), m_LongName(so->longname()),m_TypeName(so->typeName()), m_So(so), skd(NULL)
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

SkyObjItem::~SkyObjItem()
{
    delete skd;
}

QVariant SkyObjItem::data(int role)
{
    switch(role)
    {
        case DispNameRole:
            return getLongName();
        case DispImageRole:
            return getImageURL(true);
        case DispSummaryRole:
            return getSummary(true);
        case CategoryRole:
            return getType();
        case CategoryNameRole:
            return getTypeName();
        default:
            return QVariant();
    }
}

///Moved to skyobjlistmodel.cpp
/*
QHash<int, QByteArray> SkyObjItem::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[DispNameRole] = "dispName";
    roles[CategoryRole] = "type";
    roles[CategoryNameRole] = "typeName";
    return roles;
}
*/

void SkyObjItem::setPosition(SkyObject * so)
{
    KStarsData * data = KStarsData::Instance();
    KStarsDateTime ut = data->geo()->LTtoUT(KStarsDateTime(QDateTime::currentDateTime().toLocalTime()));
    SkyPoint sp = so->recomputeCoords(ut, data->geo());

    //check altitude of object at this time.
    sp.EquatorialToHorizontal(data->lst(), data->geo()->lat());
    double rounded_altitude = (int)(sp.alt().Degrees()/5.0)*5.0;

    if(rounded_altitude<=0)
        m_Position = "<span style='color:red'>" + xi18n("NOT VISIBLE: About %1 degrees below the %2 horizon", -rounded_altitude, KSUtils::toDirectionString( sp.az() ) ) + "</span>";
    else
        m_Position = "<span style='color:yellow'>" + xi18n("Now visible: About %1 degrees above the %2 horizon", rounded_altitude, KSUtils::toDirectionString( sp.az() ) ) + "</span>";
}

QString SkyObjItem::getImageURL(bool preferThumb) const
{
    if ( m_Type==Star )
    {
        return "";
    }
    QString thumbName = KSPaths::locate(QStandardPaths::GenericDataLocation, "thumb-" + m_So->name().toLower().remove( ' ' ) + ".png" ) ;
    QString fullSizeName = KSPaths::locate(QStandardPaths::GenericDataLocation, "Image_" + m_So->name().toLower().remove( ' ' ) + ".png" ) ;

    //First try to return the preferred file
    if(thumbName!=""&&preferThumb)
        return thumbName;
    if(fullSizeName!=""&&(!preferThumb))
        return fullSizeName;

    //If that fails, try to return the large image first, then the thumb and then if it is a planet, the xplanet image.
    QString fname = KSPaths::locate(QStandardPaths::GenericDataLocation, "Image_" + m_So->name().toLower().remove( ' ' ) + ".png" ) ;
    if(fname=="")
        fname = KSPaths::locate(QStandardPaths::GenericDataLocation, "thumb-" + m_So->name().toLower().remove( ' ' ) + ".png" ) ;
    if(fname=="" && m_Type==Planet){
        fname=KSPaths::locate(QStandardPaths::GenericDataLocation, "xplanet/" + m_So->name() + ".png" );
    }

    return fname;

}

QString SkyObjItem::getSummary(bool includeDescription) const
{
    if(includeDescription)
        return m_So->typeName() + "<BR>" + getRADE() + "<BR>" + getAltAz()+"<BR><BR>" + loadObjectDescription();
    else
        return m_So->typeName() + "<BR>" + getRADE() + "<BR>" + getAltAz();
}

QString SkyObjItem::getDesc() const
{
    if (m_Type == Planet)
    {
        KSFileReader fileReader;
        if (!fileReader.open("PlanetFacts.dat"))
            return xi18n("No Description found for selected sky-object");

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
        if(!skd->downloadedData().isEmpty())
            return skd->downloadedData();
    }

    if(m_Type == Star)
        return xi18n("Bright Star");

    return m_So->typeName();

}

QString SkyObjItem::getDescSource()
{
    if (m_Type == Planet)
    {
        return xi18n("(Source: Wikipedia)");
    }

    if(skd)
    {
        if(!skd->downloadedData().isEmpty() && !skd->url().isEmpty())
            return "(Source: <a href=\"" + skd->url() + "\">Wikipedia</a>)";
    }

    return xi18n("(Source: N/A)");
}


QString SkyObjItem::getSurfaceBrightness() const
{
    /** Surface Brightness is applicable only for extended light sources like
      * Deep-Sky Objects. Here we use the formula SB = m + log10(a*b/4)
      * where m is the magnitude of the sky-object. a and b are the major and minor
      * axis lengths of the objects respectively in arcminutes. SB is the surface
      * brightness obtained in mag * arcminutes^-2
      */

    DeepSkyObject * dso = (DeepSkyObject *)m_So;
    float SB = m_So->mag() + 2.5 * log10(dso->a() * dso->b() / 4);

    switch(getType())
    {
        case Galaxy:
        case Nebula:
            return QLocale().toString(SB,'f', 2) + "<BR>   (mag/arcmin^2)";
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
            return QLocale().toString(((DeepSkyObject *)m_So)->a(),'f', 2) + "\"";
        case Planet:
            return QLocale().toString(((KSPlanetBase *)m_So)->angSize(),'f', 2) + "\"";
        default:
            return QString(" --");
    }
}

inline QString SkyObjItem::loadObjectDescription() const{
    QFile file;
    QString fname = "description-" + getName().toLower().remove( ' ' ) + ".html";
    file.setFileName( KSPaths::writableLocation(QStandardPaths::GenericDataLocation) + "descriptions/" + fname ) ; //determine filename in local user KDE directory tree.

    if(file.exists())
    {
        if(file.open(QIODevice::ReadOnly))
        {
            QTextStream in(&file);
            QString line;
            line = in.readLine();//This should only read the description since the source is on the next line
            file.close();
            return line;
        }
    }
    return "";
}
