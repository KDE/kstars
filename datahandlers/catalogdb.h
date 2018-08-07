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

#pragma once

#include "ksparser.h"

#include <KLocalizedString>
#if !defined(ANDROID)
#include <KMessageBox>
#endif

#include <QSqlDatabase>
#include <QSqlError>

class SkyObject;
class CatalogComponent;
class CatalogData;
class CatalogEntryData;

/*
 * Some notes about the database. (skycomponents.sqlite)
 * 1) The uid for Object Designation is the uid being used by objects in KStars
 *    hence, the uid is a qint64 i.e. a 64 bit signed integer. Coincidentally,
 *    this is the max limit of an int in Sqlite3.
 *    Hence, the db is compatible with the uid, but doesn't use it as of now.
 */

class CatalogDB
{
  public:
    /**
     * @brief Initializes the database and sets up pointers to Catalog DB
     * Performs the following actions:
     * 1. Checks if database file exists
     * 2. Checks if database can be opened
     * 3. If DB file is missing, creates new DB
     * 4. Sets up pointer to Catalog DB
     *
     * usage: Call QSqlDatabase::removeDatabase("skydb"); after the object
     * of this class is deallocated
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
    QStringList *Catalogs();

    /**
     * @brief Rechecks the database and builds the Catalog listing.
     * New listing is directly updated into catalogs (accessed using Catalogs())
     *
     * @return void
     **/
    void RefreshCatalogList();

    /**
     * @short Parses the catalog header and returns the catalog name
     *
     * @p filename the name of the file containing the data to be read
     * @return Catalog name or empty string if there is an error
     */
    QString GetCatalogName(const QString &filename);

    /**
     * @short Add contents of custom catalog to the program database
     *
     * @p filename the name of the file containing the data to be read
     * @return true if catalog was successfully added
     */
    bool AddCatalogContents(const QString &filename);

    /**
     * @brief returns the id of the row if it matches with certain fuzz.
     * Else return -1 if none found
     *
     * @param ra Right Ascension of new object to be added
     * @param dec Declination of new object to be added
     * @return int RowUID of the new row
     **/
    int FindFuzzyEntry(const double ra, const double dec, const double magnitude);

    /**
     * @brief Removes the catalog from the database and refreshes the listing.
     *
     * @param catalog_name Name of the catalog
     * @return void
     **/
    void RemoveCatalog(const QString &catalog_name);

    /**
     * @brief Creates objects of type SkyObject and assigns them to references
     *
     * @param catalog Name of the catalog whose objects are needed.
     * @param sky_list List of all skyobjects stored in database (assigns)
     * @param names List of named objects in database (assigns)
     * @param catalog_pointer pointer to the catalogcomponent objects
     * (needed for building skyobjects)
     * @param includeCatalogDesignation This is useful when using 'fake' catalogs to bundle up
     * catalogs. Imagine a "Misc" catalog with a bunch of miscellaneous objects. We don't want
     * the designations "Misc 1", "Misc 2" etc. So the only proper designations are the
     * long name. When this is the case, this flag is set to false, and the catalog designation
     * (cat_prefix + cat_id) will not be included in the object_names returned.
     *
     * @return void
     **/
    void GetAllObjects(const QString &catalog_name, QList<SkyObject *> &sky_list,
                       QList<QPair<int, QString>> &object_names, CatalogComponent *catalog_pointer,
                       bool includeCatalogDesignation = true);

    /**
     * @brief Get information about the catalog like Prefix etc
     *
     * @param catalog_name Name of catalog whose details are required
     * @param catalog_data Data structure assigned with required data
     * @return void
     **/
    void GetCatalogData(const QString &catalog_name, CatalogData &catalog_data);

    /**
     * @brief Used to add a cross referenced entry into the database
     *
     * @note This public method opens and closes the database.
     *
     * @param catalog_entry Data structure with entry details
     * @param catid Category ID in the database
     * @return false if adding was unsuccessful
     **/
    bool AddEntry(const CatalogEntryData &catalog_entry, int catid);

    /**
     * @brief Check an entry in the database
     *
     * @note This public method opens and closes the database.
     *
     * @param entry_long_name Long name of the entry
     * @param catid Category ID in the database
     * @return True if successful otherwise false
     **/
    bool CheckCustomEntry(const QString &entry_long_name, int catid);

    /**
     * @brief Remove an entry from the database
     *
     * @note This public method opens and closes the database.
     *
     * @param entry_long_name Long name of the entry
     * @param catid Category ID in the database
     * @return True if successful otherwise false
     **/
    bool RemoveCustomEntry(const QString &entry_long_name, int catid);

    /**
     * @brief Returns database ID of the required catalog.
     * Returns -1 if not found.
     *
     * @param name Name of the class being searched
     * @return int
     **/
    int FindCatalog(const QString &catalog_name);

    /**
     * @brief Add the catalog with given details into the database
     *
     * @param catalog_data CatalogData object encompassing all catalog info
     * @return void
     **/
    void AddCatalog(const CatalogData &catalog_data);

  private:
    /**
     * @brief Used to add a cross referenced entry into the database
     *
     * @note This private method is useful when calling the method
     * repeatedly on an already-opened DB.
     *
     * @param catalog_entry Data structure with entry details
     * @param catid Category ID in the database
     * @return false if adding was unsuccessful
     **/
    bool _AddEntry(const CatalogEntryData &catalog_entry, int catid);

    /**
     * @brief Database object for the sky object. Assigned and Initialized by Initialize()
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
    bool ParseCatalogInfoToDB(const QStringList &lines, QStringList &columns, QString &catalog_name, char &delimiter);

    /**
     * @brief Prepares the sequence required by KSParser according to header.
     * Information on the sequence is stored inside the header
     *
     * @param Columns List of the columns names as strings
     * @return QList of the format usable by KSParser
     **/
    QList<QPair<QString, KSParser::DataTypes>> buildParserSequence(const QStringList &Columns);

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
