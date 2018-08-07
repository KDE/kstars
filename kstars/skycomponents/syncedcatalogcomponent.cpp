/***************************************************************************
            syncedcatalogcomponent.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Tue 16 Aug 2016 04:19:00 CDT
    copyright            : (c) 2016 by Akarsh Simha
    email                : akarsh.simha@kdemail.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

/* Project Includes */
#include "syncedcatalogcomponent.h"

#include "catalogdata.h"
#include "deepskyobject.h"
#include "kstarsdata.h"
#include "Options.h"
#include "tools/nameresolver.h"

/* KDE Includes */

/* Qt Includes */

SyncedCatalogComponent::SyncedCatalogComponent(SkyComposite *parent, const QString &catname, bool showerrs, int index)
    : CatalogComponent(parent, catname, showerrs, index, false)
{
    // First check if the catalog exists
    CatalogDB *db = KStarsData::Instance()->catalogdb();
    Q_ASSERT(db);
    m_catId = db->FindCatalog(catname);
    if (m_catId >= 0)
        loadData();
    else
    {
        // Create the catalog
        qWarning() << "Creating new catalog " << catname;
        CatalogData catData;
        catData.color =
            "#ff0000"; // FIXME: Allow users to change the color of these catalogs (eg: for resolved objects)
        catData.epoch        = 2000.0;
        catData.fluxfreq     = "400 nm";
        catData.fluxunit     = "mag";
        catData.author       = "KStars";
        catData.license      = "Unknown";
        catData.catalog_name = catname;
        catData.prefix       = catname;
        db->AddCatalog(catData);
        m_catId = db->FindCatalog(catname);
        loadData();
        Options::setShowCatalogNames(Options::showCatalogNames() << catname);
    }
    Q_ASSERT(m_catId >= 0);
    m_catColor = "#ff0000"; // FIXME: HARDCODED!
    m_catCount = m_ObjectList.count();
}

/*
SyncedCatalogComponent::~SyncedCatalogComponent() {
}
*/
/*
void SyncedCatalogComponent::draw( SkyPainter *skyp ) {
    qDebug() << "in SyncedCatalogComponent::draw()!";
    CatalogComponent::draw( skyp );
}
*/

DeepSkyObject *SyncedCatalogComponent::addObject(CatalogEntryData &catalogEntry)
{
    if (std::isnan(catalogEntry.major_axis))
        catalogEntry.major_axis = 0.0;
    if (std::isnan(catalogEntry.minor_axis))
        catalogEntry.minor_axis = 0.0;
    CatalogEntryData dbEntry = catalogEntry;
    if (dbEntry.catalog_name != m_catName)
    {
        qWarning() << "Trying to add object " << catalogEntry.catalog_name << catalogEntry.ID << " to catalog "
                   << m_catName << " will over-write catalog name with " << m_catName
                   << " in the database and assign an arbitrary ID";
        dbEntry.catalog_name = m_catName;
    }
    dbEntry.ID    = m_catCount;
    CatalogDB *db = KStarsData::Instance()->catalogdb();
    if (!(db->AddEntry(dbEntry, m_catId)))
        return nullptr;
    m_catCount++;
    qDebug() << "Added object " << catalogEntry.long_name << " into database!";
    DeepSkyObject *newObj = new DeepSkyObject(
        catalogEntry,
        this); // FIXME: What about stars? Are they treated as DeepSkyObjects, type CATALOG_STAR? -- asimha
    Q_ASSERT(newObj);

    qDebug() << "Created new DSO for " << catalogEntry.long_name << " - type: " << newObj->type();
    if (newObj->hasLongName())
    {
        //        newObj->setName( newObj->longname() );
        objectNames()[newObj->type()].append(newObj->longname());
        objectLists()[newObj->type()].append(QPair<QString, const SkyObject *>(newObj->longname(), newObj));
    }
    else
    {
        qWarning() << "Created object with name " << newObj->name() << " which is probably fake!";
        objectNames()[newObj->type()].append(newObj->name());
        objectLists()[newObj->type()].append(QPair<QString, const SkyObject *>(newObj->name(), newObj));
    }
    m_ObjectList.append(newObj);
    qDebug() << "Added new SkyObject " << newObj->name() << " to synced catalog " << m_catName << " which now contains "
             << m_ObjectList.count() << " objects.";
    return newObj;
}

bool SyncedCatalogComponent::hasObject(SkyObject &object)
{
    QString name;

    if (object.hasLongName())
    {
        name = object.longname();
    } else {
        name = object.name();
    }

    CatalogDB *db = KStarsData::Instance()->catalogdb();

    if (db->CheckCustomEntry(name, m_catId))
    {
        return true;
    }
    return false;
}

bool SyncedCatalogComponent::removeObject(SkyObject &object)
{
    QString name;

    if (object.hasLongName())
    {
        name = object.longname();
    } else {
        name = object.name();
    }
    if (objectNames()[object.type()].contains(name))
    {
        objectNames()[object.type()].removeAll(name);
        objectLists()[object.type()].removeAll(QPair<QString, const SkyObject *>(name, &object));
    } else {
        qWarning() << "Can't find SkyObject " << name << " in the synced catalog " << m_catName;
        return false;
    }
    m_ObjectList.removeAll(&object);
    qDebug() << "Remove SkyObject " << name << " from synced catalog " << m_catName;
    // Remove the catalog entry
    CatalogEntryData cedata = NameResolver::resolveName(name);

    if (!std::isnan(cedata.ra) && !std::isnan(cedata.dec))
    {
        CatalogDB *db = KStarsData::Instance()->catalogdb();

        if (!db->RemoveCustomEntry(name, m_catId))
        {
            qWarning() << "Can't find SkyObject " << name << " in the CatalogDB";
            return false;
        }
    }
    return true;
}
