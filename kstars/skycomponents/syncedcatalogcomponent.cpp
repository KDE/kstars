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
#include "catalogdb.h"
#include "kstarsdata.h"

/* KDE Includes */

/* Qt Includes */

SyncedCatalogComponent::SyncedCatalogComponent( SkyComposite *parent,
                                                const QString &catname, bool showerrs, int index )
    : CatalogComponent( parent, catname, showerrs, index, false ) {

    // First check if the catalog exists
    CatalogDB *db;
    Q_ASSERT( db = KStarsData::Instance()->catalogdb() );
    m_catId = db->FindCatalog( catname );
    if( m_catId >= 0 )
        loadData();
    else {
        // Create the catalog
        qWarning() << "Creating new catalog " << catname;
        CatalogData catData;
        catData.color = "#ff0000"; // FIXME: Allow users to change the color of these catalogs (eg: for resolved objects)
        catData.epoch = 2000.0;
        catData.fluxfreq = "400 nm";
        catData.fluxunit = "mag";
        catData.author = "KStars";
        catData.license = "Unknown";
        catData.catalog_name = catname;
        catData.prefix = catname;
        db->AddCatalog( catData );
        m_catId = db->FindCatalog( catname );
        loadData();
    }
    Q_ASSERT( m_catId >= 0 );
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

DeepSkyObject *SyncedCatalogComponent::addObject( CatalogEntryData catalogEntry ) {
    if( std::isnan( catalogEntry.major_axis ) )
        catalogEntry.major_axis = 0.0;
    if( std::isnan( catalogEntry.minor_axis ) )
        catalogEntry.minor_axis = 0.0;
    if( catalogEntry.catalog_name != m_catName ) {
        qWarning() << "Trying to add object with catalog name = " << catalogEntry.catalog_name << " to catalog " << m_catName << "! Will over-write catalog name with " << m_catName;
        catalogEntry.catalog_name = m_catName;
        // NOTE: We must fix the following before we can proceed with
        // debugging: 1. CatalogEntryData doesn't come fully filled
        // for us. We need to fill in catalog_name and
        // ID. catalog_name is easy. For ID, we must maintain a
        // running ocunter like was done in AddEntry() for
        // "Misc". Please implement this.
    }
    catalogEntry.ID = m_catCount;
    CatalogDB *db = KStarsData::Instance()->catalogdb();
    if( !( db->AddEntry( catalogEntry, m_catId ) ) )
        return 0;
    m_catCount++;
    qDebug() << "Added object " << catalogEntry.long_name << " into database!";
    DeepSkyObject *newObj = new DeepSkyObject( catalogEntry ); // FIXME: What about stars? Are they treated as DeepSkyObjects, type CATALOG_STAR? -- asimha
    Q_ASSERT( newObj );

    qDebug() << "Created new DSO for " << catalogEntry.long_name;
    if( newObj->hasLongName() ) {
        //        newObj->setName( newObj->longname() );
        objectNames()[ newObj->type() ].append( newObj->longname() );
    }
    else {
        qWarning() << "Created object with name " << newObj->name() << " which is probably fake!";
        objectNames()[ newObj->type() ].append( newObj->name() );
    }
    m_ObjectList.append( newObj );
    qDebug() << "Added new SkyObject " << newObj->name() << " to synced catalog " << m_catName << " which now contains " << m_ObjectList.count() << " objects.";
    return newObj;
}

bool SyncedCatalogComponent::selected() {
    return true; // Always selected for now.
}
