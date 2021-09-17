/*
    SPDX-FileCopyrightText: 2021 Valentin Boettcher <hiro at protagon.space; @hiro98:tchncs.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once
#include <array>
#include <QString>
#include <QStringList>

namespace CatalogsDB
{
/**
  * Holds a collection of hardcoded sql statements.
  */
namespace SqlStatements
{
/* Hack to support older sqlite versions. */
#if (QT_VERSION <= QT_VERSION_CHECK(5, 12, 0))
const QString mag_asc  = "magnitude IS NOT NULL, magnitude ASC";
const QString mag_desc = "magnitude IS NULL, magnitude DESC";
#else
const QString mag_asc  = "magnitude ASC NULLS FIRST";
const QString mag_desc = "magnitude DESC NULLS LAST";
#endif

/* constants */
const QString catalog_prefix       = "cat_";
constexpr int current_db_version   = 3;
constexpr int default_htmesh_level = 3;
constexpr int user_catalog_id      = 0;
const QString user_catalog_name    = "user";
const QString master_catalog       = "master";
const QString all_catalog_view     = "all_catalogs";
const QString colors_table         = "catalog_colors";

/* metadata */
const QString create_meta_table =
    "CREATE TABLE IF NOT EXISTS meta (version INTEGER NOT "
    "NULL, htmesh_level INTEGER NOT NULL, init INTEGER NOT NULL)";

const QString update_version = "UPDATE meta SET version = :version";
const QString get_meta       = "SELECT version, htmesh_level, init FROM meta LIMIT 1";
const QString set_meta       = "INSERT INTO meta (version, htmesh_level, init) VALUES "
                               "(:version, :htmesh_level, :init)";

/* Colors */
const QString create_colors_table =
    QString("CREATE TABLE IF NOT EXISTS %1 (catalog INTEGER NOT "
            "NULL, scheme TEXT NOT NULL, color TEXT NOT NULL, UNIQUE(catalog, scheme, "
            "color))")
    .arg(colors_table);

const QString get_colors =
    QString("SELECT catalog, scheme, color FROM %1").arg(colors_table);

const QString insert_color =
    QString("INSERT INTO %1 (catalog, scheme, color) VALUES (:catalog, :scheme, :color) "
            "ON CONFLICT(catalog, scheme, color) DO UPDATE SET color = :color")
    .arg(colors_table);

/* catalog queries */
template <typename input_iterator>
QStringList from_it(input_iterator begin, input_iterator end)
{
    QStringList field_strings{};
    std::for_each(begin, end, [&](const auto & str)
    {
        field_strings << str;
    });
    return field_strings;
}

template <typename input_iterator>
QString create_field_list(input_iterator begin, input_iterator end)
{
    QStringList field_strings{ from_it(begin, end) };

    return field_strings.join(", ");
}

template <typename input_iterator>
QString create_field_list(input_iterator begin, input_iterator end, const QString &prefix)
{
    QStringList field_strings{ from_it(begin, end) };
    QStringList prefixed_field_strings;
    std::transform(field_strings.cbegin(), field_strings.cend(),
                   std::back_inserter(prefixed_field_strings),
                   [&](const auto & str)
    {
        return prefix + str;
    });

    return prefixed_field_strings.join(", ");
}

constexpr std::array<const char *, 15> catalog_collumns =
{
    "hash",       "oid",        "type",
    "ra",         "dec",        "magnitude",
    "name",       "long_name",  "catalog_identifier",
    "major_axis", "minor_axis", "position_angle",
    "flux",       "trixel",     "catalog"
};

const auto catalog_fields =
    create_field_list(catalog_collumns.begin(), catalog_collumns.end());

constexpr std::array<const char *, 14> master_catalog_collumns = { "oid",
    "type",
    "ra",
    "dec",
    "magnitude",
    "name",
    "long_name",
    "catalog_identifier",
    "major_axis",
    "minor_axis",
    "position_angle",
    "flux",
    "trixel",
    "catalog"
};

const auto master_catalog_fields =
    create_field_list(master_catalog_collumns.begin(), master_catalog_collumns.end());

/**
 * The standard fields to query when loading objects from the db into
 * kstars.
 */
constexpr std::array<const char *, 13> dso_query_fields = { "oid",
    "type",
    "ra",
    "dec",
    "magnitude",
    "name",
    "long_name",
    "catalog_identifier",
    "major_axis",
    "minor_axis",
    "position_angle",
    "flux",
    "catalog"
};

const auto object_fields =
    create_field_list(dso_query_fields.begin(), dso_query_fields.end());

// WARN: the ordering by ID is assumed in code!
const QString get_catalog_ids =
    "SELECT id FROM catalogs WHERE enabled = 1 ORDER BY id ASC";
const QString get_all_catalog_ids = "SELECT id FROM catalogs ORDER BY id ASC";
const QString enable_disable_catalog =
    "UPDATE catalogs SET enabled = :enabled WHERE id = :id";

inline const QString move_objects(const int id_1, const int id_2)
{
    return QString("INSERT INTO cat_%1 SELECT * FROM cat_%2").arg(id_2).arg(id_1);
}

inline const QString set_catalog_all_objects(const int id)
{
    return QString("UPDATE cat_%1 SET catalog = %1 WHERE TRUE").arg(id);
}

/* views */
const QString _all_catalog_view_body =
    "SELECT %1, cl.precedence FROM %2%3 c INNER JOIN catalogs cl ON cl.id = "
    "c.catalog";

inline QString all_catalog_view_body(const QString &fields, const QString &cat_prefix,
                                     int id)
{
    return QString(_all_catalog_view_body).arg(fields).arg(cat_prefix).arg(id);
}

const QString empty_view = "SELECT NULL WHERE FALSE";

/* catalog management */
const QString _create_catalog_list_table = "CREATE TABLE IF NOT EXISTS %1 ("
        "id INTEGER PRIMARY KEY,"
        "name TEXT NOT NULL,"
        "precedence REAL NOT NULL,"
        "author TEXT DEFAULT NULL,"
        "source TEXT DEFAULT NULL,"
        "description TEXT DEFAULT NULL,"
        "mut INTEGER DEFAULT 0,"
        "version INTEGER DEFAULT -1,"
        "enabled INTEGER DEFAULT 1,"
        "color TEXT DEFAULT NULL,"
        "license TEXT DEFAULT NULL,"
        "maintainer TEXT DEFAULT NULL,"
        "timestamp DATETIME DEFAULT NULL)";

inline const QString create_catalog_registry(const QString &name)
{
    return QString(_create_catalog_list_table).arg(name);
};

const QString create_catalog_list_table = create_catalog_registry("catalogs");

const QString _insert_catalog =
    "INSERT OR IGNORE INTO %1 (id, name, mut, enabled, precedence, author, source, "
    "description, version, color, license, maintainer, timestamp) "
    "VALUES (:id, :name, :mut, :enabled, :precedence, :author, :source, :description, "
    ":version, :color, :license, :maintainer, :timestamp)";

inline const QString insert_into_catalog_registry(const QString &name)
{
    return QString(_insert_catalog).arg(name);
}

template <typename input_iterator>
inline QString create_update_list(input_iterator begin, input_iterator end)
{
    QStringList field_strings{ from_it(begin, end) };
    QStringList prefixed_field_strings;
    std::transform(field_strings.cbegin(), field_strings.cend(),
                   std::back_inserter(prefixed_field_strings),
                   [&](const auto & str)
    {
        return QString("%1 = :%1").arg(str);
    });

    return prefixed_field_strings.join(", ");
}

constexpr std::array<const char *, 8> _catalog_meta_fields =
{
    "name",  "author",  "source",     "description",
    "color", "license", "maintainer", "timestamp"
};

const QString update_catalog_meta =
    QString("UPDATE catalogs SET %1 WHERE id = :id")
    .arg(create_update_list(_catalog_meta_fields.cbegin(),
                            _catalog_meta_fields.cend()));

const QString insert_catalog = insert_into_catalog_registry("catalogs");
const QString remove_catalog  = "DELETE FROM catalogs WHERE id = :id";
const QString _drop_catalog   = "DROP TABLE cat_%1";
inline const QString drop_catalog(int id)
{
    return QString(_drop_catalog).arg(id);
}

const QString _create_catalog_table = "CREATE TABLE IF NOT EXISTS %1 ("
                                      "hash BLOB PRIMARY KEY,"
                                      "oid BLOB NOT NULL,"
                                      "type INTEGER NOT NULL,"
                                      "ra REAL NOT NULL,"
                                      "dec REAL NOT NULL,"
                                      "magnitude REAL DEFAULT NULL,"
                                      "name TEXT NOT NULL,"
                                      "long_name TEXT DEFAULT NULL,"
                                      "catalog_identifier TEXT DEFAULT NULL,"
                                      "major_axis REAL DEFAULT NULL,"
                                      "minor_axis REAL DEFAULT NULL,"
                                      "position_angle REAL DEFAULT NULL,"
                                      "flux REAL DEFAULT NULL,"
                                      "trixel INTEGER DEFAULT -1,"
                                      "res_1 BLOB DEFAULT NULL,"
                                      "res_2 BLOB DEFAULT NULL,"
                                      "res_3 BLOB DEFAULT NULL,"
                                      "res_4 BLOB DEFAULT NULL,"
                                      "catalog INTEGER NOT NULL,"
                                      "FOREIGN KEY (catalog) REFERENCES catalogs (id) "
                                      "ON DELETE CASCADE "
                                      "ON UPDATE CASCADE)";

inline QString create_catalog_table(int id)
{
    return QString(_create_catalog_table)
           .arg(QString(catalog_prefix) + QString::number(id));
}

const QString drop_master = "DROP TABLE IF EXISTS master";

const QString _create_master = "CREATE TABLE master AS "
                               "SELECT %1 FROM "
                               "all_catalogs "
                               "GROUP BY oid "
                               "ORDER BY MAX(precedence)";

const QString create_master = QString(_create_master).arg(master_catalog_fields);

const QString create_master_trixel_index =
    "CREATE INDEX master_trixel_mag ON master(trixel ASC, magnitude DESC, major_axis "
    "ASC)";

const QString create_master_mag_index =
    "CREATE INDEX master_mag ON master(magnitude ASC)";
const QString create_master_type_index =
    "CREATE INDEX master_mag_type ON master(type, magnitude ASC)";
const QString create_master_name_index =
    "CREATE INDEX master_name ON master(name "
    "COLLATE NOCASE ASC, long_name COLLATE NOCASE ASC, "
    "magnitude ASC)";

const QString get_first_catalog = "SELECT id, name, precedence, author, source, "
                                  "description, mut, enabled, version, color, license, "
                                  "maintainer, timestamp FROM catalogs LIMIT 1";

const QString get_catalog_by_id = "SELECT id, name, precedence, author, source, "
                                  "description, mut, enabled, version, color, license, "
                                  "maintainer, timestamp FROM catalogs WHERE id = :id";

const QString exists_catalog_by_id = "SELECT 1 FROM catalogs WHERE id = :id";

const QString exists_master =
    "SELECT name FROM sqlite_master WHERE type='table' AND name='master';";

/* DSO queries */
const QString _dso_by_catalog = QString("SELECT %1 FROM cat_%2").arg(catalog_fields);
inline const QString dso_by_catalog(int catalog_id)
{
    return _dso_by_catalog.arg(catalog_id);
}

// Nulls last because we load the objects in reverse :P

const QString _dso_by_trixel = "SELECT %1 FROM master WHERE trixel = "
                               ":trixel ORDER BY %2, major_axis ASC";

const QString dso_by_trixel = QString(_dso_by_trixel).arg(object_fields).arg(mag_desc);

const QString _dso_by_oid = "SELECT %1 FROM master WHERE oid = :id LIMIT 1";

const QString dso_by_oid = QString(_dso_by_oid).arg(object_fields);

inline const QString dso_by_oid_and_catalog(const int id)
{
    return QString("SELECT %1 FROM cat_%2 WHERE oid = :id LIMIT 1")
           .arg(object_fields)
           .arg(id);
};

const QString _dso_by_name =
    "SELECT %1, instr(lower(name), lower(:name)) AS in_name, instr(lower(long_name), "
    "lower(:name)) AS in_lname FROM master WHERE in_name "
    "OR in_lname "
    "ORDER BY name, long_name, "
    "%2 LIMIT :limit";

const QString _dso_by_name_exact = "SELECT %1 FROM master WHERE lower(name) = lower(:name) LIMIT 1";

const QString dso_by_name       = QString(_dso_by_name).arg(object_fields).arg(mag_asc);
const QString dso_by_name_exact = QString(_dso_by_name_exact).arg(object_fields);

inline const QString dso_by_name_and_catalog(const int id)
{
    return QString("SELECT %1 FROM cat_%2 WHERE instr(name, :name) "
                   "OR instr(long_name, :name) OR instr(catalog_identifier, :name)"
                   "ORDER BY %3 LIMIT :limit")
           .arg(object_fields)
           .arg(id)
           .arg(mag_asc);
}

const QString _dso_by_wildcard = "SELECT %1 FROM master WHERE name LIKE :wildcard LIMIT "
                                 ":limit ODRER BY CAST(name AS INTEGER)";

inline const QString dso_by_wildcard()
{
    return QString(_dso_by_wildcard).arg(object_fields);
}

inline const QString dso_general_query(const QString &where, const QString &order_by = "")
{
    auto query = QString("SELECT %1 FROM master WHERE %2").arg(object_fields).arg(where);

    if (order_by.size() > 0)
        query += " ORDER BY " + order_by;

    query += " LIMIT :limit";

    return query;
}

const QString _dso_by_maglim = "SELECT %1 FROM master WHERE magnitude < :maglim "
                               "ORDER BY %2 LIMIT :limit";

const QString dso_by_maglim = QString(_dso_by_maglim).arg(object_fields).arg(mag_asc);

inline const QString dso_in_catalog_by_maglim(const int id)
{
    return QString("SELECT %1 FROM cat_%2 WHERE magnitude < :maglim "
                   "AND type = :type ORDER BY %3 LIMIT :limit")
           .arg(object_fields)
           .arg(id)
           .arg(mag_asc);
}

const QString _dso_by_maglim_and_type =
    "SELECT %1 FROM master WHERE type = :type AND magnitude < :maglim "
    "ORDER BY %2 LIMIT :limit";

const QString dso_by_maglim_and_type =
    QString(_dso_by_maglim_and_type).arg(object_fields).arg(mag_asc);

const QString _dso_count_by_type       = "SELECT type, COUNT(*) FROM %1 GROUP BY type";
const QString dso_count_by_type_master = _dso_count_by_type.arg("master");

inline const QString dso_count_by_type(int catalog_id)
{
    return _dso_count_by_type.arg("cat_" + QString::number(catalog_id));
}

const QString _insert_dso_template = "INSERT OR REPLACE INTO cat_%3 (%1) VALUES (%2)";
const QString _insert_dso =
    QString(_insert_dso_template)
    .arg(catalog_fields)
    .arg(create_field_list(catalog_collumns.begin(), catalog_collumns.end(), ":"));

inline const QString insert_dso(int catalog_id)
{
    return _insert_dso.arg(catalog_id);
}

const QString _remove_dso{ "DELETE FROM cat_%1 WHERE oid = :oid" };
inline const QString remove_dso(const int id)
{
    return _remove_dso.arg(id);
}

} // namespace SqlStatements
} // namespace CatalogsDB
