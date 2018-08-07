/***************************************************************************
             syncedcatalogcomponent.h  -  K Desktop Planetarium
                             -------------------
    begin                : Tue 16 Aug 2016 04:13:56 CDT
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

#pragma once

#include "catalogcomponent.h"
#include "catalogentrydata.h"

class DeepSkyObject;
class SkyComposite;

/**
 * @class SyncedCatalogComponent
 *
 * @short A subclass of CatalogComponent that supports run-time
 * insertion of data, which it keeps synced with the database.
 *
 * @author Akarsh Simha <akarsh@kde.org>
 */

class SyncedCatalogComponent : public CatalogComponent
{
  public:
    /**
     * @short Constructor
     *
     * @param parent Parent item
     * @param catname Catalog name
     * @param showerrs Show errors
     * @param index Index
     */
    SyncedCatalogComponent(SkyComposite *parent, const QString &catname, bool showerrs, int index);

    //    virtual void draw( SkyPainter *skyp );

    /**
     * @short Create and insert a SkyObject from the given CatalogEntryData
     *
     * @return 0 upon failure, a valid SkyObject pointer otherwise.
     */
    DeepSkyObject *addObject(CatalogEntryData &catalogEntry);

    /**
     * @short Whether an object is in the database
     *
     * @return True if the object is in this database otherwise false.
     */
    bool hasObject(SkyObject &object);

    /**
     * @short Remove an object from the database
     *
     * @return True if the object is removed otherwise false.
     */
    bool removeObject(SkyObject &object);

    /**
     * @short Edit an existing skyobject
     *
     * @note Will implement when required.
     * @note Question: How do we match the object? Probably by internal catalog id.
     */
    // bool editObject( SkyObject *object );

    void loadData() override { _loadData(false); }

    //    virtual bool selected();

  private:
    int m_catId { 0 };
    int m_catCount { 0 };
};
