/*
    SPDX-FileCopyrightText: 2012 Samikshan Bairagya <samikshan@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "skyobjitem.h"

#include "catalogobject.h"
#include "ksfilereader.h"
#include "kspaths.h"
#include "ksplanetbase.h"
#include "kstarsdata.h"
#include "ksutils.h"

SkyObjItem::SkyObjItem(SkyObject *so)
    : m_Name(so->name()), m_LongName(so->longname()), m_TypeName(so->typeName()), m_So(so)
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

QVariant SkyObjItem::data(int role)
{
    switch (role)
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

void SkyObjItem::setPosition(SkyObject *so)
{
    double altitude;
    dms azimuth;
    if (so->type() == SkyObject::SATELLITE)
    {
        altitude = so->alt().Degrees();
        azimuth  = so->az();
    }
    else
    {
        KStarsData *data  = KStarsData::Instance();
        KStarsDateTime ut = data->geo()->LTtoUT(
            KStarsDateTime(QDateTime::currentDateTime().toLocalTime()));
        SkyPoint sp = so->recomputeCoords(ut, data->geo());

        //check altitude of object at this time.
        sp.EquatorialToHorizontal(data->lst(), data->geo()->lat());
        altitude = sp.alt().Degrees();
        azimuth  = sp.az();
    }

    double rounded_altitude = (int)(altitude / 5.0) * 5.0;

    if (rounded_altitude <= 0)
        m_Position = "<span style='color:red'>" +
                     xi18n("NOT VISIBLE: About %1 degrees below the %2 horizon",
                           -rounded_altitude, KSUtils::toDirectionString(azimuth)) +
                     "</span>";
    else
        m_Position = "<span style='color:yellow'>" +
                     xi18n("Now visible: About %1 degrees above the %2 horizon",
                           rounded_altitude, KSUtils::toDirectionString(azimuth)) +
                     "</span>";
}

QString findImage(const QString &prefix, const SkyObject &obj, const QString &suffix)
{
    static const auto base =
        KSPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDirIterator search(
        base,
        QStringList() << prefix + obj.name().toLower().remove(' ').remove('/') + suffix,
        QDir::Files, QDirIterator::Subdirectories);

    return search.hasNext() ? QUrl::fromLocalFile(search.next()).url() : "";
}
QString SkyObjItem::getImageURL(bool preferThumb) const
{
    const auto &thumbURL = findImage("thumb-", *m_So, ".png");

    const auto &fullSizeURL = findImage("image-", *m_So, ".png");
    const auto &wikiImageURL =
        QUrl::fromLocalFile(KSPaths::locate(QStandardPaths::AppDataLocation,
                                            "descriptions/wikiImage-" +
                                                m_So->name().toLower().remove(' ') +
                                                ".png"))
            .url();
    QString XPlanetURL =
        QUrl::fromLocalFile(KSPaths::locate(QStandardPaths::AppDataLocation,
                                            "xplanet/" + m_So->name() + ".png"))
            .url();

    //First try to return the preferred file
    if (!thumbURL.isEmpty() && preferThumb)
        return thumbURL;
    if (!fullSizeURL.isEmpty() && (!preferThumb))
        return fullSizeURL;

    //If that fails, try to return the large image first, then the thumb, and then if it is a planet, the xplanet image. Finally if all else fails, the wiki image.
    QString fname = fullSizeURL;

    if (fname.isEmpty())
    {
        fname = thumbURL;
    }
    if (fname.isEmpty() && m_Type == Planet)
    {
        fname = XPlanetURL;
    }
    if (fname.isEmpty())
    {
        fname = wikiImageURL;
    }
    return fname;
}

QString SkyObjItem::getSummary(bool includeDescription) const
{
    if (includeDescription)
    {
        QString description = loadObjectDescription();
        if(description.indexOf(".") > 0) //This will shorten the description in the list to just a sentence, whereas in the larger space of the Object Information Summary, it is a full paragraph.
           return m_So->typeName() + "<BR>" + getRADE() + "<BR>" + getAltAz() + "<BR><BR>" + description.left(description.indexOf(".") + 1);
        else
            return m_So->typeName() + "<BR>" + getRADE() + "<BR>" + getAltAz() + "<BR><BR>" + description;
    }
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

    auto *dso = dynamic_cast<CatalogObject*>(m_So);
    float SB           = m_So->mag();

    if (dso != nullptr)
        SB += 2.5 * log10(dso->a() * dso->b() / 4);

    switch (getType())
    {
        case Galaxy:
        case Nebula:
            return QLocale().toString(SB, 'f', 2) + "<BR>   (mag/arcmin^2)";
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
            return QLocale().toString(((CatalogObject *)m_So)->a(), 'f', 2) + "\"";
        case Planet:
            return QLocale().toString(((KSPlanetBase *)m_So)->angSize(), 'f', 2) + "\"";
        default:
            return QString(" --");
    }
}

inline QString SkyObjItem::loadObjectDescription() const
{
    QFile file;
    QString fname = "description-" + getName().toLower().remove(' ') + ".html";
    //determine filename in local user KDE directory tree.
    file.setFileName(QDir(KSPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("descriptions/" + fname));

    if (file.exists())
    {
        if (file.open(QIODevice::ReadOnly))
        {
            QTextStream in(&file);
            QString line;
            line = in.readLine(); //This should only read the description since the source is on the next line
            file.close();
            return line;
        }
    }
    return getTypeName();
}

QString SkyObjItem::getRADE() const
{
    return "RA: " + m_So->ra().toHMSString() + "<BR>DE: " + m_So->dec().toDMSString();
}

QString SkyObjItem::getAltAz() const
{
    return "Alt: " + QString::number(m_So->alt().Degrees(), 'f', 2) +
           ", Az: " + QString::number(m_So->az().Degrees(), 'f', 2);
}

float SkyObjItem::getMagnitude() const
{
    return m_So->mag();
}
