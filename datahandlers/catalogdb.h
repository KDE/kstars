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
#include "kstars/skyobjects/starobject.h"
#include "kstars/skyobjects/deepskyobject.h"
#include "kstars/skycomponents/skycomponent.h"
/* Some notes about the database. (skycomponents.db)
 * 1) The uid for Object Designation is the uid being used by objects in KStars
 *    hence, the uid is a qint64 i.e. a 64 bit signed integer. Coincidentaly,
 *    this is the max limit of an int in Sqlite3.
 *    
 */

class CatalogDB {
 public:
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
                const QString &long_name, const QString& ra,
                const QString& dec, const int type,
                const float magnitude, const int position_angle,
                const float major_axis, const float minor_axis,
                const float flux);

  /**
   * @brief Removes the catalog from the database and refreshes the listing.
   *
   * @param catalog_name Name of the catalog
   * @return void
   **/
  void RemoveCatalog(const QString& catalog_name);

  /**
   * @brief Creates objects of type SkyObject and assigns them to references
   * 
   * @param catalog Name of the catalog whose objects are needed.
   * @param sky_list List of all skyobjects stored in database (assigns)
   * @param names List of named objects in database (assigns)
   * @param catalog_pointer pointer to the catalogcomponent objects
   *                        (needed for building skyobjects)
   * @return void
   **/
  void GetAllObjects(const QString &catalog,
                     QList< SkyObject* > &sky_list,
                     QMap <int, QString> &names,
                     CatalogComponent *catalog_pointer);

  /**
   * @brief Get information about the catalog like Prefix etc
   *
   * @param catalog_name Name of catalog whose details are required
   * @param prefix Prefix of the catalog (assigns)
   * @param color Color in which the objects are to be drawn (assigns)
   * @param fluxfreq Flux Frequency of the catalog (assigns)
   * @param fluxunit Flux Unit of the catalog (assigns)
   * @param epoch Epoch of the catalog (assigns)
   * @return void
   **/
  void GetCatalogData(const QString& catalog_name, QString &prefix,
                      QString &color, QString &fluxfreq,
                      QString &fluxunit, float &epoch);

  private:
  /**
   * @brief Database object for the sky object. Assigned and Initialized by
   *        Initialize()
   **/
  QSqlDatabase skydb_;

  /**
   * @brief Returns the last error the database encountered
   *
   * @return QSqlError
   **/
  QSqlError LastError();

  /**
   * @brief List of all the catalogs contained in the database.
   * This variable is accessed through Catalogs()
   **/
  QStringList catalog_list_;

  /**
   * @brief Returns database ID of the required catalog.
   * Returns -1 if not found.
   *
   * @param name Name of the class being searched
   * @return int
   **/
  int FindCatalog(const QString &name);

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

  /**
   * @brief Add the catalog with given details into the database
   *
   * @param catalog_name Name to be given to the catalog
   * @param prefix Prefix of the catalog
   * @param color Color of the drawn objects
   * @param epoch Epoch of the catalog
   * @param fluxfreq Flux Frequency of the catalog
   * @param fluxunit Flux Unit of the catalog
   * @param author Author of the catalog. Defaults to "KStars Community".
   * @param license License for the catalog. Defaults to "None".
   * @return void
   **/
  void AddCatalog(const QString& catalog_name, const QString& prefix,
                  const QString& color, const float epoch,
                  const QString& fluxfreq, const QString& fluxunit,
                  const QString& author = "KStars Community",
                  const QString& license = "None");
  /**
   * @brief Prepares the sequence required by KSParser according to header.
   * Information on the sequence is stored inside the header
   *
   * @param Columns List of the columns names as strings
   * @return QList of the format usable by KSParser
   **/
  QList< QPair< QString, KSParser::DataTypes > >
                              buildParserSequence(const QStringList& Columns);
  void ClearDSOEntries(int catalog_id);
  void FirstRun();
};

#endif  // CATALOGDB_H
