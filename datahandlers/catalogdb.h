/***************************************************************************
                catalogDB.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 2012/03/08
    copyright            : (C) 2012 by Rishab Arora
    email                : ra.rishab@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef CATALOGDB_H
#define CATALOGDB_H

#include <kstandarddirs.h>
#include <klocale.h>
#include <KDebug>
#include <QSqlDatabase>
#include <QDebug>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlTableModel>
#include <QString>
#include <QHash>
#include <QSqlError>
#include <QVariant>
#include <QFile>

/* Some notes about the database. (skycomponents.db)
 * 1) The uid for Object Designation is the uid being used by objects in KStars
 *    hence, the uid is a qint64 i.e. a 64 bit signed integer. Coincidentaly,
 *    this is the max limit of an int in Sqlite3.
 *    
 */

class CatalogDB
{
  bool Initialize();
  ~CatalogDB();
  QStringList* Catalogs();
  void RefreshCatalogList();
private:
  QSqlDatabase skydb_;
  QSqlError LastError();
  QStringList catalog_list_;
};

#endif // CATALOGDB_H
