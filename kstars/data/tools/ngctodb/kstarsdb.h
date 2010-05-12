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

#define KSTARS_DBFILE "kstars.db"
#include <QSqlDatabase>
#include <QDebug>
#include <QSqlQuery>
#include <QString>
#include <QSqlError>
#include <QVariant>

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
    bool createDefaultDatabase(QString dbfile);
    bool createDefaultDatabase();

    /** Singleton instance of the class */
    static KStarsDB *pinstance;

private:
    QSqlDatabase db;
    QSqlQuery query;
};

#endif //KSTARSDB_H_
