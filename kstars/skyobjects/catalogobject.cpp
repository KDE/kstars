/*
    SPDX-FileCopyrightText: 2001 Jason Harris <jharris@30doradus.org>
    SPDX-FileCopyrightText: 2021 Valentin Boettcher <hiro at protagon.space; @hiro98:tchncs.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "catalogobject.h"
#include "ksutils.h"
#include "Options.h"
#include "skymap.h"
#include "texturemanager.h"
#include "kstarsdata.h"
#include "kspopupmenu.h"
#include "catalogsdb.h"
#include <QCryptographicHash>
#include <typeinfo>

CatalogObject *CatalogObject::clone() const
{
    Q_ASSERT(typeid(this) == typeid(static_cast<const CatalogObject *>(
                                 this))); // Ensure we are not slicing a derived class
    return new CatalogObject(*this);      // NOLINT, (b.c. returning raw memory is bad!)
}

float CatalogObject::e() const
{
    if (m_major_axis == 0.0 || m_minor_axis == 0.0)
        return 1.0; //assume circular
    return m_minor_axis / m_major_axis;
}

double CatalogObject::labelOffset() const
{
    //Calculate object size in pixels
    double major_axis = m_major_axis;
    double minor_axis = m_minor_axis;

    if (major_axis == 0.0 && type() == 1) //catalog stars
    {
        major_axis = 1.0;
        minor_axis = 1.0;
    }

    double size =
        ((major_axis + minor_axis) / 2.0) * dms::PI * Options::zoomFactor() / 10800.0;

    return 0.5 * size + 4.;
}

QString CatalogObject::labelString() const
{
    QString oName;
    if (Options::showDeepSkyNames())
    {
        if (Options::deepSkyLongLabels() && translatedLongName() != translatedName())
            oName = translatedLongName() + " (" + translatedName() + ')';
        else
            oName = translatedName();
    }

    if (Options::showDeepSkyMagnitudes())
    {
        if (Options::showDeepSkyNames())
            oName += ' ';
        oName += '[' + QLocale().toString(mag(), 'f', 1) + "m]";
    }

    return oName;
}

SkyObject::UID CatalogObject::getUID() const
{
    return m_object_id.toLongLong(); // = qint64
}

void CatalogObject::JITupdate()
{
    KStarsData *data{ KStarsData::Instance() };

    if (m_updateID != data->updateID())
    {
        m_updateID = data->updateID();

        if (m_updateNumID != data->updateNumID())
        {
            updateCoords(data->updateNum());
            m_updateNumID = data->updateNumID();
        }

        EquatorialToHorizontal(data->lst(), data->geo()->lat());
    }
}

void CatalogObject::initPopupMenu(KSPopupMenu *pmenu)
{
#ifndef KSTARS_LITE
    pmenu->createCatalogObjectMenu(this);
#else
    Q_UNUSED(pmenu);
#endif
}

const CatalogsDB::Catalog CatalogObject::getCatalog() const
{
    if (m_database_path.get().length() == 0)
        return {};

    CatalogsDB::DBManager db{ m_database_path };

    const auto &success = db.get_catalog(m_catalog_id);
    return (success.first ? success.second : CatalogsDB::Catalog{});
}

const CatalogObject::oid CatalogObject::getId() const

{
    return CatalogObject::getId(SkyObject::TYPE(type()), ra0().Degrees(),
                                dec0().Degrees(), name(), catalogIdentifier());
}

const CatalogObject::oid CatalogObject::getId(const SkyObject::TYPE type, const double ra,
                                              const double dec, const QString &name,
                                              const QString &catalog_identifier)
{
    QString data;
    data += QString::number(type);
    data += QString::number(static_cast<int>(std::floor(ra)));
    data += QString::number(static_cast<int>(std::floor(dec)));
    data += name;
    data += catalog_identifier;

    QCryptographicHash hash{ QCryptographicHash::Sha256 };
    hash.addData(data.toUtf8());

    return hash.result();
}

void CatalogObject::load_image()
{
    if (!m_image_loaded)
    {
        QString tname  = name().toLower().remove(' ');
        m_image        = TextureManager::getImage(tname);
        m_image_loaded = true;
    }
}
