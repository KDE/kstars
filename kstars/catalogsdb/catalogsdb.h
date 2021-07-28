/***************************************************************************
                  catalogsdb.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2021-06-03
    copyright            : (C) 2021 by Valentin Boettcher
    email                : hiro at protagon.space; @hiro98:tchncs.de
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

#include <QSqlDatabase>
#include <QSqlError>
#include <exception>
#include <QString>
#include <QList>
#include <catalogsdb_debug.h>
#include <QSqlQuery>
#include <QMutex>
#include <utility>
#include "catalogobject.h"
#include "nan.h"
#include "typedef.h"

namespace CatalogsDB
{
/**
 * A simple struct to hold information about catalogs.
 */
struct Catalog
{
    /**
     * The catalog id.
     */
    int id = -1;

    /**
     * The catalog mame.
     */
    QString name = "Unknown";

    /**
     * The precedence level of a catalog.
     *
     * If doublicate objects exist in the database, the one from the
     * catalog with the highest precedence winns.
     */
    double precedence = 0;

    /**
     * The author of the catalog.
     */
    QString author = "";

    /**
     * The catalog source.
     */
    QString source = "";

    /**
     * A (short) description for the catalog.
     * QT html is allowed.
     */
    QString description = "";

    /**
     * Wether the catalog is mutable.
     */
    bool mut = false;

    /**
     * Wether the catalog is enabled.
     */
    bool enabled = false;

    /**
     * The catalog version.
     */
    int version = -1;

    /**
     * The catalog color.
     */
    QString color = "";

    /**
     * The catalog license.
     */
    QString license = "";

    /**
     * The catalog maintainer.
     */
    QString maintainer = "";

    /**
     * Build time of the catalog. Usually only catalogs with the same
     * timestamp can be considered dedublicated.
     *
     * A `null` timestamp indicates that the catalog has not been
     * built by the catalog repository.
     */
    QDateTime timestamp{};
};

const Catalog cat_defaults{};

/**
 * Holds statistical information about the objects in a catalog.
 */
struct CatalogStatistics
{
    std::map<SkyObject::TYPE, int> object_counts;
    int total_count = 0;
};

const QString db_file_extension = "kscat";
constexpr int application_id    = 0x4d515158;
constexpr int custom_cat_min_id = 1000;
constexpr int user_catalog_id   = 0;
constexpr float default_maglim  = 99;
const QString flux_unit         = "mag";
const QString flux_frequency    = "400 nm";

/**
 * Manages the catalog database and provides an interface to provide
 * an interface to query and modify the database.
 *
 * The class manages a database connection which is assumed to be
 * working (invariant). If the database can't be accessed a
 * DatabaseError is thrown upon construction. The manager is designed
 * to hold as little state as possible because the database should be
 * the single source of truth. Prepared statements are made class
 * members, only if they are performance critical.
 *
 * Most methods in this class are thread safe.
 */
class DBManager
{
  public:
    /**
     * Constructs a database manager from the \p filename which is
     * resolved to a path in the kstars data directory.
     *
     * The constructor resolves the path to the database, opens it
     * (throws if that does not work), checks the database version
     * (throws if that does not match), initializes the database,
     * registers the user catalog and updates the all_catalog_view.
     */
    DBManager(const QString &filename);
    DBManager(const DBManager &other);

    DBManager &operator=(DBManager other)
    {
        using std::swap;
        DBManager tmp{ other };

        m_db_file = other.m_db_file;
        swap(m_db, other.m_db);
        swap(m_q_cat_by_id, other.m_q_cat_by_id);
        swap(m_q_obj_by_trixel, other.m_q_obj_by_trixel);
        swap(m_q_obj_by_name, other.m_q_obj_by_name);
        swap(m_q_obj_by_name_exact, other.m_q_obj_by_name_exact);
        swap(m_q_obj_by_maglim, other.m_q_obj_by_maglim);
        swap(m_q_obj_by_maglim_and_type, other.m_q_obj_by_maglim_and_type);
        swap(m_q_obj_by_oid, other.m_q_obj_by_oid);

        return *this;
    };

    ~DBManager()
    {
        m_db.commit();
        m_db.close();
    }

    using CatalogObjectList = std::list<CatalogObject>;

    /**
     * @return the filename of the database
     */
    const QString &db_file_name() const { return m_db_file; };

    /**
     * @return wether the catalog with the \p id has been found and
     * the catalog.
     *
     * @todo use std::optional when transitioning to c++17
     */
    const std::pair<bool, Catalog> get_catalog(const int id);

    /**
     * \return a vector with all catalogs from the database. If \p
     * include_disabled is `true`, disabled catalogs will be included.
     */
    const std::vector<Catalog> get_catalogs(bool include_disabled = false);

    /**
     * @return true if the catalog with \p id exists
     *
     * @todo use std::optional when transitioning to c++17
     */
    bool catalog_exists(const int id);

    /**
     * @return return a vector of objects in the trixel with \p id.
     */
    std::vector<CatalogObject> get_objects_in_trixel(const int trixel);

    /**
     * \brief Find an objects by name.
     *
     * This will search the `name`, `long_name` and `catalog_identifier`
     * fields in all enabled catalogs for \p `name` and then return a new
     * instance of `CatalogObject` sourced from the master catalog.
     *
     * \param limit Upper limit to the quanitity of results. `-1` means "no
     * limit"
     * \param exactMatchOnly If true, the supplied name must match exactly
     *
     * \return a list of matching objects
     */
    CatalogObjectList find_objects_by_name(const QString &name, const int limit = -1,
                                           const bool exactMatchOnly = false);

    /**
     * \brief Find an objects by name in the catalog with \p `catalog_id`.
     *
     * \return a list of matching objects
     */
    CatalogObjectList find_objects_by_name(const int catalog_id, const QString &name,
                                           const int limit = -1);

    /**
     * \brief Find an objects by searching the name four wildcard. See
     * the LIKE sqlite statement.
     *
     * \return a list of matching objects
     */
    CatalogObjectList find_objects_by_wildcard(const QString &wildcard,
                                               const int limit = -1);
    /**
     * \brief Find an objects by searching the master catlog with a
     * query like `SELECT ... FROM master WHERE \p where ORDER BY \p
     * order_by ...`.
     *
     * To be used if performance does not matter (much).
     * \p order_by can be ommitted.
     *
     * \return wether the query was successful, an error message if
     * any and a list of matching objects
     */
    std::tuple<bool, const QString, CatalogObjectList>
    general_master_query(const QString &where, const QString &order_by = "",
                         const int limit = -1);

    /**
     * \brief Get an object by \p `oid`. Optinally a \p `catalog_id` can be speicfied.
     *
     * \returns if the object was found and the object itself
     */
    std::pair<bool, CatalogObject> get_object(const CatalogObject::oid &oid);
    std::pair<bool, CatalogObject> get_object(const CatalogObject::oid &oid,
                                              const int catalog_id);

    /**
     * Get \p limit objects with magnitude smaller than \p maglim (smaller =
     * brighter) from the database.
     */
    CatalogObjectList get_objects(float maglim = default_maglim, int limit = -1);

    /**
     * Get \p limit objects of \p type with magnitude smaller than \p
     * maglim (smaller = brighter) from the database. Optionally one
     * can filter by \p `catalog_id`.
     */
    CatalogObjectList get_objects(SkyObject::TYPE type, float maglim = default_maglim,
                                  int limit = -1);
    /**
     * Get \p limit objects from the catalog with \p `catalog_id` of
     * \p type with magnitude smaller than \p maglim (smaller =
     * brighter) from the database. Optionally one can filter by \p
     * `catalog_id`.
     */
    CatalogObjectList get_objects_in_catalog(SkyObject::TYPE type, const int catalog_id,
                                             float maglim = default_maglim,
                                             int limit    = -1);

    /**
     * @return return the htmesh level used by the catalog db
     */
    int htmesh_level() const { return m_htmesh_level; };

    /**
     * \brief Enable or disable a catalog.
     * \return `true` in case of succes, `false` and an error message in case
     * of an error
     *
     * This will recreate the master table.
     */
    std::pair<bool, QString> set_catalog_enabled(const int id, const bool enabled);

    /**
     * \brief remove a catalog
     * \return `true` in case of succes, `false` and an error message
     * in case of an error
     *
     * This will recreate the master table.
     */
    std::pair<bool, QString> remove_catalog(const int id);

    /**
     * Add a `CatalogObject` to a table with \p `catalog_id`. For the rest of
     * the arguments see `CatalogObject::CatalogObject`.
     *
     * \returns wether the operation was successful and if not, an error
     * message
     */
    std::pair<bool, QString> add_object(const int catalog_id, const SkyObject::TYPE t,
                                        const CachingDms &r, const CachingDms &d,
                                        const QString &n, const float m = NaN::f,
                                        const QString &lname              = QString(),
                                        const QString &catalog_identifier = QString(),
                                        const float a = 0.0, const float b = 0.0,
                                        const double pa = 0.0, const float flux = 0);

    /**
     * Add the \p `object` to a table with \p `catalog_id`. For the
     * rest of the arguments see `CatalogObject::CatalogObject`.
     *
     * \returns wether the operation was successful and if not, an
     * error message
     */
    std::pair<bool, QString> add_object(const int catalog_id, const CatalogObject &obj);

    /**
     * Add the \p `objects` to a table with \p `catalog_id`. For the
     * rest of the arguments see `CatalogObject::CatalogObject`.
     *
     * \returns wether the operation was successful and if not, an
     * error message
     */
    std::pair<bool, QString> add_objects(const int catalog_id,
                                         const std::vector<CatalogObject> &objects);

    /**
     * Remove the catalog object with the \p `oid` from the catalog with the
     * \p `catalog_id`.
     *
     * Refreshes the master catalog.
     *
     * \returns wether the operation was successful and if not, an
     * error message
     */
    std::pair<bool, QString> remove_object(const int catalog_id,
                                           const CatalogObject::oid &id);

    /**
     * Dumps the catalog with \p `id` into the file under the path \p
     * `file_path`.  This file can then be imported with
     * `import_catalog`.  If the file already exists, it will be
     * overwritten.
     *
     * The `user_version` and `application_id` pragmas are set to special
     * values, but otherwise the dump format is equal to the internal
     * database format.
     *
     * \returns wether the operation was successful and if not, an error
     * message
     */
    std::pair<bool, QString> dump_catalog(int catalog_id, QString file_path);

    /**
     * Loads a dumped catalog from path \p `file_path`. Will overwrite
     * an existing catalog if \p `overwrite` is set to true. Immutable
     * catalogs are overwritten by default.
     *
     * Checks if the pragma `application_id` matches
     * `CatalogsDB::application_id` and the pragma `user_version` to match
     * the database format version.
     *
     * \returns wether the operation was successful and if not, an error
     * message
     */
    std::pair<bool, QString> import_catalog(const QString &file_path,
                                            const bool overwrite = false);
    /**
     * Registers a new catalog in the database.
     *
     * For the parameters \sa Catalog. The catalog gets inserted into
     * `m_catalogs`. The `all_catalog_view` is updated.
     *
     * \return true in case of success, false in case of an error
     * (along with the error)
     */
    std::pair<bool, QString>
    register_catalog(const int id, const QString &name, const bool mut,
                     const bool enabled, const double precedence,
                     const QString &author      = cat_defaults.author,
                     const QString &source      = cat_defaults.source,
                     const QString &description = cat_defaults.description,
                     const int version          = cat_defaults.version,
                     const QString &color       = cat_defaults.color,
                     const QString &license     = cat_defaults.license,
                     const QString &maintainer  = cat_defaults.maintainer,
                     const QDateTime &timestamp = cat_defaults.timestamp);

    std::pair<bool, QString> register_catalog(const Catalog &cat);

    /**
     * Update the metatadata \p `catalog`.
     *
     * The updated fields are: title, author, source, description.
     *
     * \return true in case of success, false in case of an error
     * (along with the error).
     */
    std::pair<bool, QString> update_catalog_meta(const Catalog &cat);

    /**
     * Clone objects from the catalog with \p `id_1` to another with `id_2`. Useful to create a
     * custom catalog from an immutable one.
     */
    std::pair<bool, QString> copy_objects(const int id_1, const int id_2);

    /**
     * Finds the smallest free id for a catalog.
     */
    int find_suitable_catalog_id();

    /**
     * \returns statistics about the master catalog.
     */
    const std::pair<bool, CatalogStatistics> get_master_statistics();

    /**
     * \returns statistics about the catalog with \p `catalog_id`.
     */
    const std::pair<bool, CatalogStatistics> get_catalog_statistics(const int catalog_id);

    /**
     * Compiles the master catalog by merging the individual catalogs based
     * on `oid` and precedence and creates an index by (trixel, magnitude) on
     * the master table. **Caution** you may want to call
     * `update_catalog_views` beforhand.
     *
     * @return true in case of success, false in case of an error
     */
    bool compile_master_catalog();

    /**
     * Updates the all_catalog_view so that it includes all known
     * catalogs.
     *
     * @return true in case of success, false in case of an error
     */
    bool update_catalog_views();

  private:
    /**
     * The backing catalog database.
     */
    QSqlDatabase m_db;

    /**
     * The filename of the database.
     */
    QString m_db_file;

    //@{
    /**
     * Some performance criticall sql queries are prepared stored as memebers.
     * When using those queries `m_mutex` should be locked!
     *
     * \sa prepare_queries
     */

    QSqlQuery m_q_cat_by_id;
    QSqlQuery m_q_obj_by_trixel;
    QSqlQuery m_q_obj_by_name;
    QSqlQuery m_q_obj_by_name_exact;
    QSqlQuery m_q_obj_by_maglim;
    QSqlQuery m_q_obj_by_maglim_and_type;
    QSqlQuery m_q_obj_by_oid;
    //@}

    /**
     * The level of the htmesh used to index the catalog entries.
     *
     * If the htmesh level of a catalog is different, the catalog will
     * be reindexed upon importing it.
     *
     * A value of -1 means that the htmesh-level has not been
     * deterined yet.
     */
    int m_htmesh_level = -1;

    /**
     * The version of the database.
     *
     * A value of -1 means that the htmesh-level has not been
     * deterined yet.
     */
    int m_db_version = -1;

    /**
     * A simple mutex to be locked when using prepared statements,
     * that are stored in the class.
     */
    QMutex m_mutex;

    //@{
    /**
     * Helpers
     */

    /**
     * Initializes the database with the minimum viable tables.
     *
     * The catalog registry is created and the database version is set
     * to SqlStatements::current_db_version and the htmesh-level is
     * set to SqlStatements::default_htmesh_level if they don't exist.
     *
     * @return true in case of success, false in case of an error
     */
    bool initialize_db();

    /**
     * Reads the database version and the htmesh level from the
     * database. If the meta table does not exist, the default vaulues
     * SqlStatements::current_db_version and
     * SqlStatements::default_htmesh_level.
     *
     * @return [version, htmesh-level, is-init?]
     */
    std::tuple<int, int, bool> get_db_meta();

    /**
     * Gets a vector of catalog ids of catalogs. If \p include_disabled is
     * `true`, disabled catalogs will be included.
     */
    std::vector<int> get_catalog_ids(bool include_enabled = false);

    /**
     * Prepares performance critical sql queries.
     *
     * @return [success, error]
     */
    std::pair<bool, QSqlError> prepare_queries();

    /**
     * Read a `CatalogObject` from the tip of the \p query.
     */
    CatalogObject read_catalogobject(const QSqlQuery &query) const;

    /**
     * Read the first `CatalogObject` from the tip of the \p `query`
     * that hasn't been exec'd yet.
     */
    std::pair<bool, CatalogObject> read_first_object(QSqlQuery &query) const;

    /**
     * Read all `CatalogObject`s from the \p query.
     */
    CatalogObjectList fetch_objects(QSqlQuery &query) const;

    /**
     * Internal implementation to forcably remove a catalog (even the user catalog, use with caution!)
     */
    std::pair<bool, QString> remove_catalog_force(const int id);
    //@}
};

/**
 * Database related error, thrown when database access fails or an
 * action does not succeed.
 *
 * QSqlError is not used here to encapsulate the database further.
 */
class DatabaseError : std::exception
{
  public:
    enum class ErrorType
    {
        OPEN,
        VERSION,
        INIT,
        CREATE_CATALOG,
        CREATE_MASTER,
        NOT_FOUND,
        PREPARE,
        UNKNOWN
    };

    DatabaseError(QString message, ErrorType type = ErrorType::UNKNOWN,
                  const QSqlError &error = QSqlError())
        : m_message{ std::move(message) }, m_type{ type }, m_error{ error }, m_report{
              m_message.toStdString() +
              (error.text().length() > 0 ? "\nSQL ERROR: " + error.text().toStdString() :
                                           std::string(""))
          } {};

    const char *what() const noexcept override { return m_report.c_str(); }
    const QString &message() const noexcept { return m_message; }
    ErrorType type() const noexcept { return m_type; }

  private:
    const QString m_message;
    const ErrorType m_type;
    const QSqlError m_error;
    const std::string m_report;
};

/** \returns the path to the dso database */
QString dso_db_path();

/** \returns true and a catalog if the catalog metadata (name, author,
    ...) can be read */
std::pair<bool, Catalog> read_catalog_meta_from_file(const QString &path);
} // namespace CatalogsDB
