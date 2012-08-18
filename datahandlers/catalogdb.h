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
#include <KMessageBox>
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
#include <QDir>
#include <datahandlers/ksparser.h>
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
  /**
    * @short Add contents of custom catalog to the program database
    *
    * @p filename the name of the file containing the data to be read
    * @return true if catalog was successfully added
  */
  bool AddCatalogContents(const QString &filename);

  void AddEntry(const QString &catalog_name, const int ID,
                const QString &long_name, const double ra,
                const double dec, const int type,
                const float magnitude, const int position_angle,
                const float major_axis, const float minor_axis,
                const float flux);
                

private:
  QSqlDatabase skydb_;
  QSqlError LastError();
  QStringList catalog_list_;
  bool FindByName(const QString &name);

  /**
   * @short Add the catalog name and details to the db.
   * This does not store the contents. It only adds the catalog info
   * to the database. Hence, it is step 1 in AddCatalogContents
   *
   * @param lines List of lines to use for extraction of details
   * @param Columns Stores the read Columns in this list
   * @return bool
   **/
  bool ParseCatalogInfoToDB(const QStringList &lines, QStringList &columns,
                            QString &catalog_name);
  
  // TODO(spacetime): Documentation !!
  static QList< QPair< QString, KSParser::DataTypes > > 
                            buildParserSequence(const QStringList& Columns);


};

#endif // CATALOGDB_H
