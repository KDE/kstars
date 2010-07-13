/***************************************************************************
                          kstarsdb.h  -  K Desktop Planetarium
                             -------------------
    begin                : Wed May 12 2010
    copyright            : (C) 2010 by Victor Carbune
    email                : victor.carbune@kdemail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef KSTARSDB_H_
#define KSTARSDB_H_

#define KSTARS_DBFILE "data/kstars.db"
#define IMG_URL  0
#define INFO_URL 1

#include <QSqlDatabase>
#include <QDebug>
#include <QSqlQuery>
#include <QString>
#include <QHash>
#include <QSqlError>
#include <QVariant>
#include "skyobjects/skyobject.h"


class KStarsDB
{
protected:
    /** Constructor */
    KStarsDB();

public:
    /* Should this class be a Singleton? */
    static KStarsDB *Create();
    static KStarsDB *Instance();

    /** Initialize KStarsDB while running splash screen
     * @return true on success 
     */
    bool initialize();

    /** Load database from memory
     * @return true on success
     */
    bool loadDatabase(QString dbfile); 
    
    /** Create default database structure
     * @return true on success
     */
    bool createDefaultDatabase(QString);

    /** Load the database information from the standard
     * NGCIC.dat file used by KStars 
     */
    void migrateData(QString);

    /** 
     * Migrate the URL Data from image_url.dat
     */
    void migrateURLData(const QString &, int);

    /**
     * Get the SkyObject * for the object having the provided id
     */
    SkyObject *getObject(qlonglong);

    /**
     * Get the object id for the provided id
     */
    qlonglong getSkyObjectIDByName(QString);

    /** Singleton instance of the class */
    static KStarsDB *pinstance;


private:
    /** Hash of SkyObjects (database id) */
    QHash<qlonglong, SkyObject *> objectHash;

    bool createDefaultDatabase();
    
    QSqlDatabase db;
    QSqlQuery query;
};

#endif //KSTARSDB_H_
