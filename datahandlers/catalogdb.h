/***************************************************************************
                catalogDB.h  -  K Desktop Planetarium
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

#include <datahandlers/ksparser.h>
#include "kstars/skyobjects/starobject.h"
#include "kstars/skyobjects/deepskyobject.h"
#include "kstars/skycomponents/skycomponent.h"
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


/* Some notes about the database. (skycomponents.db)
 * 1) The uid for Object Designation is the uid being used by objects in KStars
 *    hence, the uid is a qint64 i.e. a 64 bit signed integer. Coincidentaly,
 *    this is the max limit of an int in Sqlite3.
 *    Hence, the db is compatible with the uid, but doesn't use it as of now.
 */

class CatalogDB {
 public:
   /**
    * @brief Initializes the database and sets up pointers to Catalog DB
    * Performs the following actions:
    * 1. Checks if database file exists
    * 2. Checks if database can be opened
    * 3. If DB file is missing, creates new DB
    * 4. Sets up pointer to Catalog DB
    *
    * @return bool
    **/
   bool Initialize();
   /**
    * @brief Attempt to close database and remove reference from the DB List
    *
    **/
   ~CatalogDB();
   /**
    * @brief Accessor for list of all available catalogs in db
    *
    * @return QStringList*
    **/
   QStringList* Catalogs();
   /**
    * @brief Rechecks the database and builds the Catalog listing.
    * New listing is directly updated into catalogs (accessed using Catalogs())
    *
    * @return void
    **/
   void RefreshCatalogList();
  /**
    * @short Add contents of custom catalog to the program database
    *
    * @p filename the name of the file containing the data to be read
    * @return true if catalog was successfully added
  */
  bool AddCatalogContents(const QString &filename);

  /**
   * @brief Used to add a cross referenced entry into the database
   *
   * @param catalog_name Name of the Catalog (must exist prior to execution)
   * @param ID The ID number from the catalog. eg. for M 31, ID is 31
   * @param long_name long name (if any) of the object
   * @param ra Right Ascension of the object (in HH:MM:SS format)
   * @param dec Declination of the object (in +/-DD:MM:SS format)
   * @param type type of the object (from skyqpainter::drawDeepSkySymbol())
   * 0: general star (not to be used)
   * 1: star
   * 2: planet
   * 3: Open Cluster
   * 4: Globular Cluster
   * 5: Gaseous Nebula
   * 6: Planetary Nebula
   * 7: Supernova remnant
   * 8: Galaxy
   * 13: Asterism
   * 14: Galaxy cluster
   * 15: Dark Nebula
   * 16: Quasar
   * @param magnitude Apparent Magnitude of the object
   * @param position_angle Position Angle of the object
   * @param major_axis Major Axis Length (arcmin)
   * @param minor_axis Minor Axis Length (arcmin)
   * @param flux Flux for the object
   * @return void
   **/
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
  void GetAllObjects(const QString &catalog_name,
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
   * @param catalog_name Name retrieved from file header
   * @param delimiter Delimeter retrieved from file header
   * @return bool
   **/
  bool ParseCatalogInfoToDB(const QStringList &lines, QStringList &columns,
                            QString &catalog_name, char &delimiter);

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
  /**
   * @brief Clears out the DSO table for the given catalog ID
   *
   * @param catalog_id DB generated catalog ID
   * @return void
  **/
  void ClearDSOEntries(int catalog_id);
  /**
   * @brief Contains setup routines to intitialize a database for catalog storage
   *
   * @return void
   **/
  void FirstRun();
};

#endif  // CATALOGDB_H
