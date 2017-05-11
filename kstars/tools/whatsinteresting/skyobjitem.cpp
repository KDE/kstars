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

SkyObjItem::SkyObjItem(SkyObject * so) : m_Name(so->name()), m_LongName(so->longname()),m_TypeName(so->typeName()), m_So(so)
{
    switch (so->type())
    {
        case SkyObject::PLANET:
        case SkyObject::MOON:
            m_Type = Planet;
            break;
        case SkyObject::STAR:
        case SkyObject::CATALOG_STAR:
        case SkyObject::MULT_STAR:
            m_Type = Star;
            break;
        case SkyObject::CONSTELLATION:
        case SkyObject::ASTERISM:
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
        case SkyObject::SUPERNOVA_REMNANT:
        case SkyObject::GASEOUS_NEBULA:
        case SkyObject::DARK_NEBULA:
            m_Type = Nebula;
            break;
        case SkyObject::SUPERNOVA:
            m_Type = Supernova;
    }

    setPosition(m_So);
}

SkyObjItem::~SkyObjItem()
{

}

QVariant SkyObjItem::data(int role)
{
    switch(role)
    {
        case DispNameRole:
            return getDescName();
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
    double altitude;
    dms azimuth;
    if(so->type()==SkyObject::SATELLITE)
    {
        altitude = so->alt().Degrees();
        azimuth = so->az();
    }
    else
    {

        KStarsData * data = KStarsData::Instance();
        KStarsDateTime ut = data->geo()->LTtoUT(KStarsDateTime(QDateTime::currentDateTime().toLocalTime()));
        SkyPoint sp = so->recomputeCoords(ut, data->geo());

        //check altitude of object at this time.
        sp.EquatorialToHorizontal(data->lst(), data->geo()->lat());
        altitude = sp.alt().Degrees();
        azimuth = sp.az();
    }

    double rounded_altitude = (int)(altitude/5.0)*5.0;

    if(rounded_altitude<=0)
        m_Position = "<span style='color:red'>" + xi18n("NOT VISIBLE: About %1 degrees below the %2 horizon", -rounded_altitude, KSUtils::toDirectionString( azimuth ) ) + "</span>";
    else
        m_Position = "<span style='color:yellow'>" + xi18n("Now visible: About %1 degrees above the %2 horizon", rounded_altitude, KSUtils::toDirectionString( azimuth ) ) + "</span>";
}

QString SkyObjItem::getImageURL(bool preferThumb) const
{
    QString thumbURL = QUrl::fromLocalFile(KSPaths::locate(QStandardPaths::GenericDataLocation, "thumb-" + m_So->name().toLower().remove( ' ' ) + ".png" )).url();
    QString fullSizeURL = QUrl::fromLocalFile(KSPaths::locate(QStandardPaths::GenericDataLocation, "image-" + m_So->name().toLower().remove( ' ' ) + ".png" )).url();
    QString wikiImageURL=QUrl::fromLocalFile(KSPaths::locate(QStandardPaths::GenericDataLocation, "descriptions/wikiImage-" + m_So->name().toLower().remove( ' ' ) + ".png" )).url();
    QString XPlanetURL = QUrl::fromLocalFile(KSPaths::locate(QStandardPaths::GenericDataLocation, "xplanet/" + m_So->name() + ".png" )).url();

    //First try to return the preferred file
    if(thumbURL!=""&&preferThumb)
        return thumbURL;
    if(fullSizeURL!=""&&(!preferThumb))
        return fullSizeURL;

    //If that fails, try to return the large image first, then the thumb, and then if it is a planet, the xplanet image. Finally if all else fails, the wiki image.
    QString fname = fullSizeURL ;
    if(fname == "")
        fname = thumbURL ;
    if(fname == "" && m_Type==Planet){
        fname = XPlanetURL;
    }
    if(fname == ""){
        fname = wikiImageURL;
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
    return getTypeName();
}
