/***************************************************************************
                  catalogsdb.cpp  -  K Desktop Planetarium
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

#include <limits>
#include <cmath>
#include <QSqlDriver>
#include <QSqlRecord>
#include <QMutexLocker>
#include "cachingdms.h"
#include "catalogsdb.h"
#include "kspaths.h"
#include "skymesh.h"
#include "Options.h"
#include "final_action.h"
#include "sqlstatements.h"

using namespace CatalogsDB;

/**
 * Get an increasing index for new connections.
 */
int get_connection_index()
{
    static int connection_index = 0;
    return connection_index++;
}

DBManager::DBManager(const QString &filename)
    : m_db{ QSqlDatabase::addDatabase(
          "QSQLITE", QString("cat_%1_%2").arg(filename).arg(get_connection_index())) },
      m_db_file{ filename }, m_q_cat_by_id{ m_db, SqlStatements::get_catalog_by_id,
                                            true },
      m_q_obj_by_trixel{ m_db, SqlStatements::dso_by_trixel, false },
      m_q_obj_by_name{ m_db, SqlStatements::dso_by_name, true },
      m_q_obj_by_name_exact{ m_db, SqlStatements::dso_by_name_exact, true },
      m_q_obj_by_maglim{ m_db, SqlStatements::dso_by_maglim, true },
      m_q_obj_by_maglim_and_type{ m_db, SqlStatements::dso_by_maglim_and_type, true },
      m_q_obj_by_oid{ m_db, SqlStatements::dso_by_oid, true }
{
    m_db.setDatabaseName(m_db_file);

    // we are throwing here, because errors at this stage should be fatal
    if (!m_db.open())
    {
        throw DatabaseError(QString("Cannot open CatalogDatabase '%1'!").arg(m_db_file),
                            DatabaseError::ErrorType::OPEN, m_db.lastError());
    }

    bool init                                    = false;
    std::tie(m_db_version, m_htmesh_level, init) = get_db_meta();

    if (m_db_version > 0 && m_db_version != SqlStatements::current_db_version)
    {
        throw DatabaseError(QString("Wrong database version. Expected %1 and got %2.")
                                .arg(SqlStatements::current_db_version)
                                .arg(m_db_version),
                            DatabaseError::ErrorType::VERSION, m_db.lastError());
    }

    QSqlQuery master_exists{ m_db };
    master_exists.exec(SqlStatements::exists_master);

    if (init || master_exists.size() <= 0)
    {
        if (!initialize_db())
        {
            throw DatabaseError(QString("Could not initialize database."),
                                DatabaseError::ErrorType::INIT, m_db.lastError());
        }

        if (!catalog_exists(SqlStatements::user_catalog_id))
        {
            const auto &res =
                register_catalog(SqlStatements::user_catalog_id,
                                 SqlStatements::user_catalog_name, true, true, 1);
            if (!res.first)
            {
                throw DatabaseError(QString("Could not create user database."),
                                    DatabaseError::ErrorType::CREATE_CATALOG, res.second);
            }
        }

        if (!update_catalog_views())
        {
            throw DatabaseError(QString("Unable to create combined catalog view!"),
                                DatabaseError::ErrorType::CREATE_CATALOG,
                                m_db.lastError());
        }

        if (!compile_master_catalog())
        {
            throw DatabaseError(QString("Unable to create master catalog!"),
                                DatabaseError::ErrorType::CREATE_MASTER,
                                m_db.lastError());
        }
    }
};

DBManager::DBManager(const DBManager &other) : DBManager::DBManager{ other.m_db_file } {};

bool DBManager::initialize_db()
{
    if (m_db_version < 0 || m_htmesh_level < 1)
        throw std::runtime_error("DBManager not initialized properly, m_db_vesion and "
                                 "m_htmesh_level have to be set.");

    if (!m_db.exec(SqlStatements::create_meta_table).isActive())
        return false;

    QSqlQuery meta_query{ m_db };
    meta_query.prepare(SqlStatements::set_meta);
    meta_query.bindValue(0, m_db_version);
    meta_query.bindValue(1, m_htmesh_level);
    meta_query.bindValue(2, false);

    if (!meta_query.exec())
        return false;

    return m_db.exec(SqlStatements::create_catalog_list_table).isActive();
}

std::tuple<int, int, bool> DBManager::get_db_meta()
{
    auto query = m_db.exec(SqlStatements::get_meta);

    if (query.first())
        return { query.value(0).toInt(), query.value(1).toInt(),
                 query.value(2).toBool() };
    else
        return { SqlStatements::current_db_version, SqlStatements::default_htmesh_level,
                 true };
}

std::vector<int> DBManager::get_catalog_ids(bool include_disabled)
{
    auto query = m_db.exec(include_disabled ? SqlStatements::get_all_catalog_ids :
                                              SqlStatements::get_catalog_ids);

    std::vector<int> ids;

    while (query.next())
    {
        int id = query.value(0).toInt();
        ids.push_back(id);
    }

    return ids;
}

bool DBManager::update_catalog_views()
{
    const auto &ids = get_catalog_ids();
    bool result     = true;
    auto _          = gsl::finally([&]() { m_db.commit(); });

    m_db.transaction();
    QSqlQuery query{ m_db };
    result &=
        query.exec(QString("DROP VIEW IF EXISTS ") + SqlStatements::all_catalog_view);

    if (!result)
    {
        return result;
    }

    QString view{
        "CREATE VIEW  "
    }; // small enough to be included here and not in sqlstatements

    view += SqlStatements::all_catalog_view;
    view += " AS\n";

    QStringList prefixed{};
    for (auto *field : SqlStatements::catalog_collumns)
    {
        prefixed << QString("c.") + field;
    }

    QString prefixed_joined = prefixed.join(",");

    QStringList catalog_queries{};
    for (auto id : ids)
    {
        catalog_queries << SqlStatements::all_catalog_view_body(
            prefixed_joined, SqlStatements::catalog_prefix, id);
    }

    if (ids.size() == 0)
    {
        catalog_queries << SqlStatements::all_catalog_view_body(
                               prefixed_joined, SqlStatements::catalog_prefix, 0) +
                               " WHERE FALSE"; // we blackhole the query
    }

    view += catalog_queries.join("\nUNION ALL\n");
    result &= query.exec(view);
    return result;
}

void bind_catalog(QSqlQuery &query, const Catalog &cat)
{
    query.bindValue(":id", cat.id);
    query.bindValue(":name", cat.name);
    query.bindValue(":mut", cat.mut);
    query.bindValue(":enabled", cat.enabled);
    query.bindValue(":precedence", cat.precedence);
    query.bindValue(":author", cat.author);
    query.bindValue(":source", cat.source);
    query.bindValue(":description", cat.description);
    query.bindValue(":version", cat.version);
    query.bindValue(":color", cat.color);
    query.bindValue(":license", cat.license);
    query.bindValue(":maintainer", cat.maintainer);
}

std::pair<bool, QString> DBManager::register_catalog(
    const int id, const QString &name, const bool mut, const bool enabled,
    const double precedence, const QString &author, const QString &source,
    const QString &description, const int version, const QString &color,
    const QString &license, const QString &maintainer)
{
    return register_catalog({ id, name, precedence, author, source, description, mut,
                              enabled, version, color, license, maintainer });
}

std::pair<bool, QString> DBManager::register_catalog(const Catalog &cat)
{
    if (catalog_exists(cat.id))
        return { false, i18n("Catalog with that ID already exists.") };

    QSqlQuery query{ m_db };

    if (!query.exec(SqlStatements::create_catalog_table(cat.id)))
    {
        return { false, query.lastError().text() };
    }

    query.prepare(SqlStatements::insert_catalog);
    bind_catalog(query, cat);

    return { query.exec(), query.lastError().text() };
};

bool DBManager::compile_master_catalog()
{
    auto _ = gsl::finally([&]() { m_db.commit(); });
    QSqlQuery query{ m_db };
    m_db.transaction();

    if (!query.exec(SqlStatements::drop_master))
    {
        return false;
    }

    if (!query.exec(SqlStatements::create_master))
    {
        return false;
    }

    bool success = true;
    success &= query.exec(SqlStatements::create_master_trixel_index);
    success &= query.exec(SqlStatements::create_master_mag_index);
    success &= query.exec(SqlStatements::create_master_type_index);
    success &= query.exec(SqlStatements::create_master_name_index);
    return success;
};

const Catalog read_catalog(const QSqlQuery &query)
{
    return { query.value("id").toInt(),
             query.value("name").toString(),
             query.value("precedence").toDouble(),
             query.value("author").toString(),
             query.value("source").toString(),
             query.value("description").toString(),
             query.value("mut").toBool(),
             query.value("enabled").toBool(),
             query.value("version").toInt(),
             query.value("color").toString(),
             query.value("license").toString(),
             query.value("maintainer").toString() };
}

const std::pair<bool, Catalog> DBManager::get_catalog(const int id)
{
    QMutexLocker _{ &m_mutex };
    m_q_cat_by_id.bindValue(0, id);

    if (!m_q_cat_by_id.exec())
        return { false, {} };

    if (!m_q_cat_by_id.next())
        return { false, {} };

    Catalog cat{ read_catalog(m_q_cat_by_id) };

    m_q_cat_by_id.finish();
    return { true, cat };
}

bool DBManager::catalog_exists(const int id)
{
    QMutexLocker _{ &m_mutex };
    m_q_cat_by_id.bindValue(0, id);
    auto end = gsl::finally([&]() { m_q_cat_by_id.finish(); });

    if (!m_q_cat_by_id.exec())
        return false;

    return m_q_cat_by_id.next();
}

size_t count_rows(QSqlQuery &query)
{
    size_t count{ 0 };
    while (query.next())
    {
        count++;
    }

    return count;
}

CatalogObject DBManager::read_catalogobject(const QSqlQuery &query) const
{
    const CatalogObject::oid id = query.value(0).toByteArray();
    const SkyObject::TYPE type  = static_cast<SkyObject::TYPE>(query.value(1).toInt());

    const double ra         = query.value(2).toDouble();
    const double dec        = query.value(3).toDouble();
    const float mag         = query.isNull(4) ? NaN::f : query.value(4).toFloat();
    const QString name      = query.value(5).toString();
    const QString long_name = query.value(6).toString();
    const QString catalog_identifier = query.value(7).toString();
    const float major                = query.value(8).toFloat();
    const float minor                = query.value(9).toFloat();
    const double position_angle      = query.value(10).toDouble();
    const float flux                 = query.value(11).toFloat();
    const int catalog_id             = query.value(12).toInt();

    return { id,         type,     dms(ra),   dms(dec),
             mag,        name,     long_name, catalog_identifier,
             catalog_id, major,    minor,     position_angle,
             flux,       m_db_file };
}

std::vector<CatalogObject> DBManager::get_objects_in_trixel(const int trixel)
{
    QMutexLocker _{ &m_mutex }; // this costs ~ .1ms which is ok
    m_q_obj_by_trixel.bindValue(0, trixel);

    if (!m_q_obj_by_trixel.exec()) // we throw because this is not recoverable
        throw DatabaseError(
            QString("The query m_by_trixel_query for objects in trixel=%1 failed.")
                .arg(trixel),
            DatabaseError::ErrorType::UNKNOWN, m_q_obj_by_trixel.lastError());

    std::vector<CatalogObject> objects;
    size_t count =
        count_rows(m_q_obj_by_trixel); // this also moves the query head to the end

    if (count == 0)
    {
        m_q_obj_by_trixel.finish();
        return objects;
    }

    objects.reserve(count);

    while (m_q_obj_by_trixel.previous())
    {
        objects.push_back(read_catalogobject(m_q_obj_by_trixel));
    }

    m_q_obj_by_trixel.finish();

    // move semantics baby!
    return objects;
}

std::list<CatalogObject> DBManager::fetch_objects(QSqlQuery &query) const
{
    std::list<CatalogObject> objects;
    auto _ = gsl::finally([&]() { query.finish(); });

    query.exec();

    if (!query.isActive())
        return {};
    while (query.next())
        objects.push_back(read_catalogobject(query));

    return objects;
}

std::list<CatalogObject> DBManager::find_objects_by_name(const QString &name,
                                                         const int limit)
{
    QMutexLocker _{ &m_mutex };

    // search for an exact match first
    if (limit == 1)
    {
        m_q_obj_by_name_exact.bindValue(":name", name);
        const auto &objs = fetch_objects(m_q_obj_by_name_exact);

        if (objs.size() > 0)
        {
            return objs;
        }
    }

    m_q_obj_by_name.bindValue(":name", name);
    m_q_obj_by_name.bindValue(":limit", limit);

    return fetch_objects(m_q_obj_by_name);
}

std::list<CatalogObject> DBManager::find_objects_by_name(const int catalog_id,
                                                         const QString &name,
                                                         const int limit)
{
    QSqlQuery query{ m_db };

    query.prepare(SqlStatements::dso_by_name_and_catalog(catalog_id));
    query.bindValue(":name", name);
    query.bindValue(":limit", limit);
    query.bindValue(":catalog", catalog_id);

    return fetch_objects(query);
}

std::pair<bool, CatalogObject> DBManager::read_first_object(QSqlQuery &query) const
{
    if (!query.exec() || !query.first())
        return { false, {} };

    return { true, read_catalogobject(query) };
}

std::pair<bool, CatalogObject> DBManager::get_object(const CatalogObject::oid &oid)
{
    QMutexLocker _{ &m_mutex };
    m_q_obj_by_oid.bindValue(0, oid);

    auto f = gsl::finally([&]() { // taken from the GSL, runs when it goes out of scope
        m_q_obj_by_oid.finish();
    });

    return read_first_object(m_q_obj_by_oid);
};

std::pair<bool, CatalogObject> DBManager::get_object(const CatalogObject::oid &oid,
                                                     const int catalog_id)
{
    QMutexLocker _{ &m_mutex };
    QSqlQuery query{ m_db };

    query.prepare(SqlStatements::dso_by_oid_and_catalog(catalog_id));
    query.bindValue(0, oid);

    return read_first_object(query);
};

std::list<CatalogObject> DBManager::get_objects(float maglim, int limit)
{
    QMutexLocker _{ &m_mutex };
    m_q_obj_by_maglim.bindValue(":maglim", maglim);
    m_q_obj_by_maglim.bindValue(":limit", limit);

    return fetch_objects(m_q_obj_by_maglim);
}

std::list<CatalogObject> DBManager::get_objects(SkyObject::TYPE type, float maglim,
                                                int limit)
{
    QMutexLocker _{ &m_mutex };
    m_q_obj_by_maglim_and_type.bindValue(":type", type);
    m_q_obj_by_maglim_and_type.bindValue(":limit", limit);
    m_q_obj_by_maglim_and_type.bindValue(":maglim", maglim);

    return fetch_objects(m_q_obj_by_maglim_and_type);
}

std::list<CatalogObject> DBManager::get_objects_in_catalog(SkyObject::TYPE type,
                                                           const int catalog_id,
                                                           float maglim, int limit)
{
    QSqlQuery query{ m_db };

    query.prepare(SqlStatements::dso_in_catalog_by_maglim(catalog_id));
    query.bindValue(":type", type);
    query.bindValue(":limit", limit);
    query.bindValue(":maglim", maglim);
    return fetch_objects(query);
}

std::pair<bool, QString> DBManager::set_catalog_enabled(const int id, const bool enabled)
{
    const auto &success = get_catalog(id);
    if (!success.first)
        return { false, i18n("Catalog could not be found.") };

    const auto &cat = success.second;
    if (cat.enabled == enabled)
        return { true, "" };

    QSqlQuery query{ m_db };
    query.prepare(SqlStatements::enable_disable_catalog);
    query.bindValue(":enabled", enabled);
    query.bindValue(":id", id);

    return { query.exec() && update_catalog_views() && compile_master_catalog(),
             query.lastError().text() + m_db.lastError().text() };
}

const std::vector<Catalog> DBManager::get_catalogs(bool include_disabled)
{
    auto ids = get_catalog_ids(include_disabled);
    std::vector<Catalog> catalogs;
    catalogs.reserve(ids.size());

    std::transform(ids.cbegin(), ids.cend(), std::back_inserter(catalogs),
                   [&](const int id) {
                       const auto &found = get_catalog(id);
                       if (found.first)
                           return found.second;

                       // This really should **not** happen
                       throw DatabaseError(
                           QString("Could not retrieve the catalog with id=%1").arg(id));
                   });

    return catalogs;
}

inline void bind_catalogobject(QSqlQuery &query, const int catalog_id,
                               const SkyObject::TYPE t, const CachingDms &r,
                               const CachingDms &d, const QString &n, const float m,
                               const QString &lname, const QString &catalog_identifier,
                               const float a, const float b, const double pa,
                               const float flux, Trixel trixel,
                               const CatalogObject::oid &new_id)
{
    query.prepare(SqlStatements::insert_dso(catalog_id));

    query.bindValue(":hash", new_id); // no dedupe, maybe in the future
    query.bindValue(":oid", new_id);
    query.bindValue(":type", static_cast<int>(t));
    query.bindValue(":ra", r.Degrees());
    query.bindValue(":dec", d.Degrees());
    query.bindValue(":magnitude", (m < 99 && !std::isnan(m)) ? m : QVariant{});
    query.bindValue(":name", n);
    query.bindValue(":long_name", lname.length() > 0 ? lname : QVariant{});
    query.bindValue(":catalog_identifier",
                    catalog_identifier.length() > 0 ? catalog_identifier : QVariant{});
    query.bindValue(":major_axis", a > 0 ? a : QVariant{});
    query.bindValue(":minor_axis", b > 0 ? b : QVariant{});
    query.bindValue(":position_angle", pa > 0 ? pa : QVariant{});
    query.bindValue(":flux", flux > 0 ? flux : QVariant{});
    query.bindValue(":trixel", trixel);
    query.bindValue(":catalog", catalog_id);
}

inline void bind_catalogobject(QSqlQuery &query, const int catalog_id,
                               const CatalogObject &obj, Trixel trixel)
{
    bind_catalogobject(query, catalog_id, static_cast<SkyObject::TYPE>(obj.type()),
                       obj.ra0(), obj.dec0(), obj.name(), obj.mag(), obj.longname(),
                       obj.catalogIdentifier(), obj.a(), obj.b(), obj.pa(), obj.flux(),
                       trixel, obj.getObjectId());
};

std::pair<bool, QString> DBManager::add_object(const int catalog_id,
                                               const CatalogObject &obj)
{
    return add_object(catalog_id, static_cast<SkyObject::TYPE>(obj.type()), obj.ra0(),
                      obj.dec0(), obj.name(), obj.mag(), obj.longname(),
                      obj.catalogIdentifier(), obj.a(), obj.b(), obj.pa(), obj.flux());
}

std::pair<bool, QString>
DBManager::add_object(const int catalog_id, const SkyObject::TYPE t, const CachingDms &r,
                      const CachingDms &d, const QString &n, const float m,
                      const QString &lname, const QString &catalog_identifier,
                      const float a, const float b, const double pa, const float flux)
{
    {
        const auto &success = get_catalog(catalog_id);
        if (!success.first)
            return { false, i18n("Catalog with id=%1 not found.", catalog_id) };

        if (!success.second.mut)
            return { false, i18n("Catalog is immutable!") };
    }

    SkyPoint tmp{ r, d };
    const auto trixel = SkyMesh::Create(m_htmesh_level)->index(&tmp);
    QSqlQuery query{ m_db };

    const auto new_id = CatalogObject::getId(t, r.Degrees(), d.Degrees(), m, a, b, pa,
                                             flux, n, lname, catalog_identifier);
    bind_catalogobject(query, catalog_id, t, r, d, n, m, lname, catalog_identifier, a, b,
                       pa, flux, trixel, new_id);

    if (!query.exec())
    {
        auto err = query.lastError().text();
        if (err.startsWith("UNIQUE"))
            err = i18n("The object is already in the catalog!");

        return { false, i18n("Could not insert object! %1", err) };
    }

    return { update_catalog_views() && compile_master_catalog(),
             m_db.lastError().text() };
}

std::pair<bool, QString> DBManager::remove_object(const int catalog_id,
                                                  const CatalogObject::oid &id)
{
    QSqlQuery query{ m_db };

    query.prepare(SqlStatements::remove_dso(catalog_id));
    query.bindValue(":oid", id);

    if (!query.exec())
        return { false, query.lastError().text() };

    return { update_catalog_views() && compile_master_catalog(),
             m_db.lastError().text() };
}

std::pair<bool, QString> DBManager::dump_catalog(int catalog_id, QString file_path)
{
    const auto &found = get_catalog(catalog_id);
    if (!found.first)
        return { false, i18n("Catalog could not be found.") };

    QFile file{ file_path };
    if (!file.open(QIODevice::WriteOnly))
        return { false, i18n("Output file is not writable.") };
    file.resize(0);
    file.close();

    QSqlQuery query{ m_db };

    if (!query.exec(QString("ATTACH [%1] AS tmp").arg(file_path)))
        return { false,
                 i18n("Could not attach output file.<br>%1", query.lastError().text()) };

    m_db.transaction();
    auto _ = gsl::finally([&]() { // taken from the GSL, runs when it goes out of scope
        m_db.commit();
        query.exec("DETACH tmp");
    });

    if (!query.exec(
            QString("CREATE TABLE tmp.cat AS SELECT * FROM cat_%1").arg(catalog_id)))
        return { false, i18n("Could not copy catalog to output file.<br>%1")
                            .arg(query.lastError().text()) };

    if (!query.exec(SqlStatements::create_catalog_registry("tmp.catalogs")))
        return { false, i18n("Could not create catalog registry in output file.<br>%1")
                            .arg(query.lastError().text()) };

    query.prepare(SqlStatements::insert_into_catalog_registry("tmp.catalogs"));

    auto cat    = found.second;
    cat.enabled = true;
    bind_catalog(query, cat);

    if (!query.exec())
    {
        return { false,
                 i18n("Could not insert catalog into registry in output file.<br>%1")
                     .arg(query.lastError().text()) };
    }

    if (!query.exec(QString("PRAGMA tmp.user_version = %1").arg(m_db_version)))
    {
        return { false, i18n("Could not insert set exported database version.<br>%1")
                            .arg(query.lastError().text()) };
    }

    if (!query.exec(QString("PRAGMA tmp.application_id = %1").arg(application_id)))
    {
        return { false,
                 i18n("Could not insert set exported database application id.<br>%1")
                     .arg(query.lastError().text()) };
    }

    return { true, {} };
}

std::pair<bool, QString> DBManager::import_catalog(const QString &file_path,
                                                   const bool overwrite)
{
    QFile file{ file_path };
    if (!file.open(QIODevice::ReadOnly))
        return { false, i18n("Catalog file is not readable.") };
    file.close();

    QSqlQuery query{ m_db };

    if (!query.exec(QString("ATTACH [%1] AS tmp").arg(file_path)))
    {
        m_db.commit();
        return { false,
                 i18n("Could not attach input file.<br>%1", query.lastError().text()) };
    }

    auto _ = gsl::finally([&]() {
        m_db.commit();
        query.exec("DETACH tmp");
    });

    if (!query.exec("PRAGMA tmp.application_id") || !query.next() ||
        query.value(0).toInt() != CatalogsDB::application_id)
        return { false, i18n("Invalid catalog file.") };

    if (!query.exec("PRAGMA tmp.user_version") || !query.next() ||
        query.value(0).toInt() != m_db_version)
        return { false,
                 i18n("Unsupported catalog format version.<br>Wanted '%1' and got '%2'.")
                     .arg(m_db_version)
                     .arg(query.value(0).toInt()) };

    if (!query.exec("SELECT id FROM tmp.catalogs LIMIT 1") || !query.next())
        return { false,
                 i18n("Could read the catalog id.<br>%1", query.lastError().text()) };

    const auto id = query.value(0).toInt();
    query.finish();

    {
        const auto &found = get_catalog(id);
        if (found.first)
        {
            if (!overwrite && found.second.mut)
                return { false, i18n("Catalog already exists in the database!") };

            auto success = remove_catalog_force(id);
            if (!success.first)
                return success;
        }
    }

    m_db.transaction();

    if (!query.exec(
            "INSERT INTO catalogs (id, name, mut, enabled, precedence, author, source, "
            "description, version, color, license, maintainer) SELECT id, name, mut, "
            "enabled, "
            "precedence, author, source, description, version, color, license, "
            "maintainer FROM "
            "tmp.catalogs LIMIT 1") ||
        !query.exec(QString("CREATE TABLE cat_%1 AS SELECT * FROM tmp.cat").arg(id)))
        return { false,
                 i18n("Could not import the catalog.<br>%1", query.lastError().text()) };

    m_db.commit();

    if (!update_catalog_views() || !compile_master_catalog())
        return { false, i18n("Could not refresh the master catalog.<br>",
                             m_db.lastError().text()) };

    return { true, {} };
}

std::pair<bool, QString> DBManager::remove_catalog(const int id)
{
    if (id == SqlStatements::user_catalog_id)
        return { false, i18n("Removing the user catalog is not allowed.") };

    return remove_catalog_force(id);
}

std::pair<bool, QString> DBManager::remove_catalog_force(const int id)
{
    auto success = set_catalog_enabled(id, false);
    if (!success.first)
        return success;

    QSqlQuery remove_catalog{ m_db };
    remove_catalog.prepare(SqlStatements::remove_catalog);
    remove_catalog.bindValue(0, id);

    m_db.transaction();

    if (!remove_catalog.exec() || !remove_catalog.exec(SqlStatements::drop_catalog(id)))
    {
        m_db.rollback();
        return { false, i18n("Could not remove the catalog from the registry.<br>%1")
                            .arg(remove_catalog.lastError().text()) };
    }

    m_db.commit();
    // we don't have to refresh the master catalog because the disable
    // call already did this

    return { true, {} };
}

std::pair<bool, QString> DBManager::copy_objects(const int id_1, const int id_2)
{
    if (!(catalog_exists(id_1) && catalog_exists(id_2)))
        return { false, i18n("Both catalogs have to exist!") };

    if (!get_catalog(id_2).second.mut)
        return { false, i18n("Destination catalog has to be mutable!") };

    QSqlQuery query{ m_db };

    if (!query.exec(SqlStatements::move_objects(id_1, id_2)))
        return { false, query.lastError().text() };

    if (!query.exec(SqlStatements::set_catalog_all_objects(id_2)))
        return { false, query.lastError().text() };

    return { true, {} };
}

std::pair<bool, QString> DBManager::update_catalog_meta(const Catalog &cat)
{
    if (!catalog_exists(cat.id))
        return { false, i18n("Cannot update nonexisting catalog.") };

    QSqlQuery query{ m_db };

    query.prepare(SqlStatements::update_catalog_meta);
    query.bindValue(":name", cat.name);
    query.bindValue(":author", cat.author);
    query.bindValue(":source", cat.source);
    query.bindValue(":description", cat.description);
    query.bindValue(":id", cat.id);
    query.bindValue(":color", cat.color);
    query.bindValue(":license", cat.license);
    query.bindValue(":maintainer", cat.maintainer);

    return { query.exec(), query.lastError().text() };
}

int DBManager::find_suitable_catalog_id()
{
    const auto &cats = get_catalogs(true);

    // find a gap in the ids to use
    const auto element = std::adjacent_find(
        cats.cbegin(), cats.cend(), [](const auto &c1, const auto &c2) {
            return (c1.id >= CatalogsDB::custom_cat_min_id) &&
                   (c2.id >= CatalogsDB::custom_cat_min_id) && (c2.id - c1.id) > 1;
        });

    return std::max(CatalogsDB::custom_cat_min_id,
                    (element == cats.cend() ? cats.back().id : element->id) + 1);
}

QString CatalogsDB::dso_db_path()
{
    return KSPaths::writableLocation(QStandardPaths::GenericDataLocation) +
           Options::dSOCatalogFilename();
}

QString CatalogsDB::init_dso_db_path()
{
    const auto &target = dso_db_path();
    if (QFile(target).exists())
        return target;

    const auto &default_file = KSPaths::locate(QStandardPaths::GenericDataLocation,
                                               Options::dSOCatalogFilename());

    if (QFile(default_file).exists())
    {
        QFile::copy(default_file, target);
    }

    return target;
}

std::pair<bool, Catalog> CatalogsDB::read_catalog_meta_from_file(const QString &path)
{
    QSqlDatabase db{ QSqlDatabase::addDatabase(
        "QSQLITE", QString("tmp_%1_%2").arg(path).arg(get_connection_index())) };
    db.setDatabaseName(path);
    if (!db.open())
        return { false, {} };

    QSqlQuery query{ db };
    if (!query.exec(SqlStatements::get_first_catalog) || !query.first())
        return { false, {} };

    db.close();
    return { true, read_catalog(query) };
}

CatalogStatistics read_statistics(QSqlQuery &query)
{
    CatalogStatistics stats{};
    while (query.next())
    {
        stats.object_counts[(SkyObject::TYPE)query.value(0).toInt()] =
            query.value(1).toInt();
        stats.total_count += query.value(1).toInt();
    }
    return stats;
}

const std::pair<bool, CatalogStatistics> DBManager::get_master_statistics()
{
    QSqlQuery query{ m_db };
    if (!query.exec(SqlStatements::dso_count_by_type_master))
        return { false, {} };

    return { true, read_statistics(query) };
}

const std::pair<bool, CatalogStatistics>
DBManager::get_catalog_statistics(const int catalog_id)
{
    QSqlQuery query{ m_db };
    if (!query.exec(SqlStatements::dso_count_by_type(catalog_id)))
        return { false, {} };

    return { true, read_statistics(query) };
}

std::pair<bool, QString>
CatalogsDB::DBManager::add_objects(const int catalog_id,
                                   const std::vector<CatalogObject> &objects)
{
    {
        const auto &success = get_catalog(catalog_id);
        if (!success.first)
            return { false, i18n("Catalog with id=%1 not found.", catalog_id) };

        if (!success.second.mut)
            return { false, i18n("Catalog is immutable!") };
    }

    m_db.transaction();
    QSqlQuery query{ m_db };
    for (const auto &object : objects)
    {
        SkyPoint tmp{ object.ra(), object.dec() };
        const auto trixel = SkyMesh::Create(m_htmesh_level)->index(&tmp);

        bind_catalogobject(query, catalog_id, object, trixel);

        if (!query.exec())
        {
            auto err = query.lastError().text();
            if (err.startsWith("UNIQUE"))
                err = i18n("The object is already in the catalog!");

            return { false, i18n("Could not insert object! %1", err) };
        }
    }

    return { m_db.commit() && update_catalog_views() && compile_master_catalog(),
             m_db.lastError().text() };
};
