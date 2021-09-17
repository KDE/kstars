/*
    SPDX-FileCopyrightText: 2021 Valentin Boettcher <hiro at protagon.space; @hiro98:tchncs.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QtTest/QtTest>
#include <QtConcurrent/QtConcurrentRun>
#include <qtestcase.h>
#include "catalogsdb.h"
#include "skymesh.h"

using namespace CatalogsDB;
class TestCatalogsDB_DBManager : public QObject
{
    Q_OBJECT
  public:
    TestCatalogsDB_DBManager() : m_manager{ getTestSqliteFile() } {}

  private:
    const QString db_file_og = QFINDTESTDATA("data/test.sqlite");
    DBManager m_manager;
    CatalogObject some_object() { return m_manager.get_objects(99, 1).front(); }
    QString getTestSqliteFile()
    {
        const QString db_file = "test.sqlite";
        QFile::remove(db_file);
        QFile::copy(db_file_og, db_file);
        return db_file;
    }

  private slots:
    void init()
    {
        // A fresh manager for each test.
        m_manager = DBManager{ getTestSqliteFile() };
    };

    void user_catalog_created()
    {
        QTemporaryFile tmp;
        m_manager = DBManager{ tmp.fileName() };
        QCOMPARE(m_manager.get_catalog(0).second.id, 0);
    }

    void getting_catalogs()
    {
        // first without disabled
        const auto &cats = m_manager.get_catalogs();
        for (const auto &cat : cats)
        {
            // the catalogs should be the same
            QCOMPARE(m_manager.get_catalog(cat.id).second.id, cat.id);
            // and they should exist
            QVERIFY(m_manager.catalog_exists(cat.id));
            QVERIFY(cat.enabled);
        }

        // and then with disabled
        const auto &cats_d = m_manager.get_catalogs(true);
        for (const auto &cat : cats_d)
        {
            // the catalogs should be the same
            QCOMPARE(m_manager.get_catalog(cat.id).second.id, cat.id);
            // and they should exist
            QVERIFY(m_manager.catalog_exists(cat.id));
        }
    }

    void getting_objects_in_trixel()
    {
        const int num_trixels = SkyMesh::Create(m_manager.htmesh_level())->size();
        int num_obj           = 0;

        QBENCHMARK
        {
            for (int trixel = 0; trixel < num_trixels; trixel++)
            {
                num_obj += m_manager.get_objects_in_trixel(trixel).size();
            }
        }

        QVERIFY(num_obj > 0);
    }

    void find_by_name()
    {
        const auto &obj  = some_object();
        const auto &objs = m_manager.find_objects_by_name(obj.name(), 1);
        QCOMPARE(objs.size(), 1);
        QCOMPARE(obj.name(), objs.front().name());
    }

    void get_by_id()
    {
        const auto &obj     = some_object();
        const auto &fetched = m_manager.get_object(obj.getObjectId());

        QVERIFY(fetched.first);
        QCOMPARE(fetched.second, obj);

        const auto &fetched_in_cat =
            m_manager.get_object(obj.getObjectId(), obj.getCatalog().id);

        QVERIFY(fetched_in_cat.first);
        QCOMPARE(fetched_in_cat.second, obj);
    }

    void get_objects_by_maglim()
    {
        const auto &objs = m_manager.get_objects(1, 10);
        QVERIFY(objs.size() <= 10);

        for (const auto &obj : objs)
        {
            QVERIFY(obj.mag() <= 1);
        }
    }

    void get_objects_by_type()
    {
        const auto &objs = m_manager.get_objects(SkyObject::SUPERNOVA_REMNANT, 1, 10);
        QVERIFY(objs.size() <= 10);

        for (const auto &obj : objs)
        {
            QVERIFY(obj.mag() <= 1);
            QCOMPARE(obj.type(), SkyObject::SUPERNOVA_REMNANT);
        }
    }

    void get_objects_by_type_in_catalog()
    {
        for (const auto &cat : m_manager.get_catalogs(true))
        {
            const auto &cat_id = cat.id;
            const auto &objs   = m_manager.get_objects_in_catalog(
                SkyObject::SUPERNOVA_REMNANT, cat_id, 1, 10);
            QVERIFY(objs.size() <= 10);

            for (const auto &obj : objs)
            {
                QVERIFY(obj.mag() <= 1);
                QCOMPARE(obj.type(), SkyObject::SUPERNOVA_REMNANT);
                QCOMPARE(obj.getCatalog().id, cat_id);
            }
        }
    }

    void enable_disable_catalogs()
    {
        for (const auto &cat : m_manager.get_catalogs(true))
        {
            for (const auto enable : { true, false })
            {
                const auto &success = m_manager.set_catalog_enabled(cat.id, enable);
                QVERIFY(success.first);
                QCOMPARE(m_manager.get_catalog(cat.id).second.enabled, enable);
            }
        }

        const auto &objs = m_manager.get_objects();
        QCOMPARE(objs.size(), 0);
    }

    void remove_catalog()
    {
        for (const auto &cat : m_manager.get_catalogs(true))
        {
            const auto size     = m_manager.get_catalogs(true).size();
            const auto &success = m_manager.remove_catalog(cat.id);
            if (cat.id == 0) // user catalog cannot be deleted
            {
                QVERIFY(!success.first);
                continue;
            }

            QVERIFY(success.first);
            QCOMPARE(m_manager.get_catalogs(true).size(), size - 1);
        }
    }

    void register_dump_and_read_catalog()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto &path = QString("%1/%2").arg(dir.path()).arg("tmp.sql");

        const auto &names = { "a", "b", "c" };
        const int id      = m_manager.find_suitable_catalog_id();

        const auto &now = QDateTime::currentDateTime();
        auto success =
            m_manager.register_catalog(id, "test", true, true, 1, "me", "test suite",
                                       "test catalog", 2, "green", "gpl", "me", now);
        QVERIFY(success.first);

        for (const auto &name : names)
            m_manager.add_object(id, SkyObject::STAR, dms{ 0 }, dms{ 0 }, name);

        const auto &objects = m_manager.get_objects_in_catalog(SkyObject::STAR, id);

        success = m_manager.dump_catalog(id, path);
        QVERIFY2(success.first, "Dumping catalogs.");

        const auto cat_meta = CatalogsDB::read_catalog_meta_from_file(path);

        QVERIFY2(cat_meta.first, "Read dumped catalog meta data.");
        QCOMPARE(cat_meta.second.id, id);
        QCOMPARE(cat_meta.second.name, "test");
        QCOMPARE(cat_meta.second.precedence, 1);
        QCOMPARE(cat_meta.second.author, "me");
        QCOMPARE(cat_meta.second.mut, true);
        QCOMPARE(cat_meta.second.enabled, true);
        QCOMPARE(cat_meta.second.source, "test suite");
        QCOMPARE(cat_meta.second.description, "test catalog");
        QCOMPARE(cat_meta.second.version, 2);
        QCOMPARE(cat_meta.second.color, "green");
        QCOMPARE(cat_meta.second.license, "gpl");
        QCOMPARE(cat_meta.second.maintainer, "me");
        QCOMPARE(cat_meta.second.timestamp, now);

        success = m_manager.import_catalog(path);
        QVERIFY2(!success.first, "Not overwriting an existing catalog.");

        success = m_manager.import_catalog(path, true);
        QVERIFY2(success.first, "Importing and overwriting."); // import and overwrite

        success = m_manager.remove_catalog(id); // remove and import fresh
        QVERIFY(!m_manager.get_catalog(id).first);

        success = m_manager.import_catalog(path);
        QVERIFY2(success.first, "Importing fresh.");

        const auto &new_objects = m_manager.get_objects_in_catalog(SkyObject::STAR, id);
        for (const auto &object : objects)
        {
            QVERIFY2(std::find(new_objects.cbegin(), new_objects.cend(), object) !=
                         new_objects.cend(),
                     "Object found in reimported catalog.");
        }
    }

    void edit_catalog()
    {
        const Catalog cat = {
            0, "New User", 0, "test", "toast", "toaster", false, false
        };

        auto success = m_manager.update_catalog_meta(cat);
        QVERIFY2(success.first, "Changing meta works.");
        QCOMPARE(m_manager.get_catalog(0).second.name, "New User");
        QCOMPARE(m_manager.get_catalog(0).second.author, "test");
        QCOMPARE(m_manager.get_catalog(0).second.source, "toast");
        QCOMPARE(m_manager.get_catalog(0).second.description, "toaster");
        QCOMPARE(m_manager.get_catalog(0).second.mut, true); // did not change

        success = m_manager.update_catalog_meta({ -1, "bla" });
        QVERIFY2(!success.first, "Changing meta of nonexisting catalog doesn't work.");
    }

    void register_from_catalog_objetct()
    {
        const Catalog cat{ m_manager.find_suitable_catalog_id(),
                           "test",
                           1,
                           "tester",
                           "test catalog",
                           "testing catalog",
                           true,
                           false,
                           100 };

        auto success = m_manager.register_catalog(cat);
        QVERIFY2(success.first, "Registering a catalog worked.");

        const auto &success_fetch = m_manager.get_catalog(cat.id);
        QVERIFY2(success_fetch.first, "Inserted catalog found.");

        const auto &cat_new = success_fetch.second;
        QCOMPARE(cat_new.id, cat.id);
        QCOMPARE(cat_new.name, cat.name);
        QCOMPARE(cat_new.precedence, cat.precedence);
        QCOMPARE(cat_new.author, cat.author);
        QCOMPARE(cat_new.source, cat.source);
        QCOMPARE(cat_new.description, cat.description);
        QCOMPARE(cat_new.enabled, cat.enabled);
        QCOMPARE(cat_new.version, cat.version);
    }

    void add_object_and_copy()
    {
        const Catalog cat{ m_manager.find_suitable_catalog_id(),
                           "test",
                           1,
                           "tester",
                           "test catalog",
                           "testing catalog",
                           true,
                           false,
                           100 };

        auto success = m_manager.register_catalog(cat);
        QVERIFY2(success.first, "Registering a catalog worked.");

        auto success_add =
            m_manager.add_object(cat.id, SkyObject::STAR, dms{ 1 }, dms{ 0 }, "test", -1,
                                 "long test", "NGC 100", 10, 11, 111, 0);
        QVERIFY2(success_add.first, "Inserting an object worked.");

        const auto obj =
            m_manager.get_objects_in_catalog(SkyObject::STAR, cat.id, 99, 1).front();

        QCOMPARE(obj.type(), SkyObject::STAR);
        QCOMPARE(obj.ra0(), dms{ 1 });
        QCOMPARE(obj.dec0(), dms{ 0 });
        QCOMPARE(obj.name(), "test");
        QCOMPARE(obj.mag(), -1);
        QCOMPARE(obj.longname(), "long test");
        QCOMPARE(obj.catalogIdentifier(), "NGC 100");
        QCOMPARE(obj.a(), 10);
        QCOMPARE(obj.b(), 11);
        QCOMPARE(obj.pa(), 111);
        QCOMPARE(obj.flux(), 0);

        success_add = m_manager.add_object(cat.id, obj);
        QVERIFY2(success_add.first, "Can add the same object twice = update!");
        QCOMPARE(m_manager.get_objects_in_catalog(SkyObject::STAR, cat.id, 99, 1).size(),
                 1);

        const CatalogObject obj_direct{
            {},          SkyObject::STAR, dms{ 1 }, dms{ 11 }, -1,  "test_1",
            "long test", "NGC 100",       10,       11,        111, 0
        };

        success_add = m_manager.add_object(cat.id, obj_direct);
        QVERIFY2(success_add.first, "Can add predefined catalog object.");

        const auto obj_retr = m_manager.find_objects_by_name(cat.id, "test_1", 1).front();
        QCOMPARE(obj_direct.type(), obj_retr.type());
        QCOMPARE(obj_direct.ra0(), obj_retr.ra0());
        QCOMPARE(obj_direct.dec0(), obj_retr.dec0());
        QCOMPARE(obj_direct.name(), obj_retr.name());
        QCOMPARE(obj_direct.mag(), obj_retr.mag());
        QCOMPARE(obj_direct.longname(), obj_retr.longname());
        QCOMPARE(obj_direct.catalogIdentifier(), obj_retr.catalogIdentifier());
        QCOMPARE(obj_direct.a(), obj_retr.a());
        QCOMPARE(obj_direct.b(), obj_retr.b());
        QCOMPARE(obj_direct.pa(), obj_retr.pa());
        QCOMPARE(obj_direct.flux(), obj_retr.flux());

        const auto &success_copy = m_manager.copy_objects(cat.id, 0);
        QVERIFY2(success_copy.first, "Can copy objects.");

        QCOMPARE(m_manager.find_objects_by_name(0, "test", 1).size(), 1);
        QCOMPARE(m_manager.find_objects_by_name(0, "test", 1).front().getCatalog().id, 0);

        QCOMPARE(m_manager.find_objects_by_name(0, "test_1", 1).size(), 1);
        QCOMPARE(m_manager.get_objects_in_catalog(SkyObject::STAR, 0).size(), 2);
    }

    void add_and_remove_object()
    {
        auto success =
            m_manager.add_object(0, SkyObject::STAR, dms{ 0 }, dms{ 0 }, "tester");
        QVERIFY(success.first);
        QCOMPARE(m_manager.find_objects_by_name(0, "tester", 1).front().name(), "tester");

        const CatalogObject o{ {}, SkyObject::STAR, dms{ 0 }, dms{ 0 }, 0, "tester_1" };
        success = m_manager.add_object(0, o);
        QVERIFY(success.first);
        QCOMPARE(m_manager.find_objects_by_name(0, "tester_1", 1).front().getId(), o);

        success = m_manager.remove_object(0, o.getObjectId());
        QVERIFY(success.first);
        QCOMPARE(m_manager.find_objects_by_name(0, "tester_1", 1).size(), 0);
    }

    void add_objects()
    {
        const Catalog cat{ m_manager.find_suitable_catalog_id(),
                           "test",
                           1,
                           "tester",
                           "test catalog",
                           "testing catalog",
                           true,
                           false,
                           100 };

        auto success = m_manager.register_catalog(cat);
        QVERIFY2(success.first, "Registering a catalog worked.");

        const int num_objs{ 1000 };
        std::vector<float> test_mags(num_objs);
        std::iota(test_mags.begin(), test_mags.end(), 1);
        std::vector<CatalogObject> objects;
        std::transform(
            test_mags.begin(), test_mags.end(), std::back_inserter(objects),
            [](const auto mag) {
                return CatalogObject{ {},          SkyObject::STAR,
                                      dms{ 1 },    dms{ 11 },
                                      -1,          QString("test_%1_").arg(mag),
                                      "long test", "NGC 100",
                                      10,          11,
                                      111,         0 };
            });

        auto success_add = m_manager.add_objects(cat.id, objects);
        QVERIFY2(success_add.first, "Inserting an object worked.");
        QCOMPARE(m_manager.get_catalog_statistics(cat.id).second.total_count, num_objs);

        for (const auto &obj : objects)
        {
            const auto obj_retr =
                m_manager.find_objects_by_name(cat.id, obj.name(), 1).front();
            QCOMPARE(obj.type(), obj_retr.type());
            QCOMPARE(obj.ra0(), obj_retr.ra0());
            QCOMPARE(obj.dec0(), obj_retr.dec0());
            QCOMPARE(obj.name(), obj_retr.name());
            QCOMPARE(obj.mag(), obj_retr.mag());
            QCOMPARE(obj.longname(), obj_retr.longname());
            QCOMPARE(obj.catalogIdentifier(), obj_retr.catalogIdentifier());
            QCOMPARE(obj.a(), obj_retr.a());
            QCOMPARE(obj.b(), obj_retr.b());
            QCOMPARE(obj.pa(), obj_retr.pa());
            QCOMPARE(obj.flux(), obj_retr.flux());
        }
    }

    void concurrent_query()
    {
        auto f1 = QtConcurrent::run([&] {
            const auto &objs = m_manager.get_objects();
            return objs.size() > 0;
        });

        auto f2 = QtConcurrent::run([&] {
            const auto &objs = m_manager.get_objects();
            return objs.size() > 0;
        });

        f1.waitForFinished();
        f2.waitForFinished();

        QVERIFY(f1.result() && f2.result());
    }

    void statistics()
    {
        auto success_add =
            m_manager.add_object(user_catalog_id, SkyObject::STAR, dms{ 1 }, dms{ 0 },
                                 "test", -1, "long test", "NGC 100", 10, 11, 111, 0);
        QVERIFY2(success_add.first, "Adding object succeeds.");

        success_add = m_manager.add_object(user_catalog_id, SkyObject::SUPERNOVA_REMNANT,
                                           dms{ 1 }, dms{ 0 }, "test", -1, "long test",
                                           "NGC 100", 10, 11, 111, 0);
        QVERIFY2(success_add.first, "Adding object succeeds.");

        auto stats = m_manager.get_catalog_statistics(0);
        QVERIFY2(stats.first, "retrieving stats works");
        QCOMPARE(stats.second.total_count, 2);
        QCOMPARE(stats.second.object_counts.at(SkyObject::STAR), 1);
        QCOMPARE(stats.second.object_counts.at(SkyObject::SUPERNOVA_REMNANT), 1);

        stats = m_manager.get_master_statistics();
        QVERIFY2(stats.first, "retrieving stats works");
        QVERIFY(stats.second.total_count > 0);
        QVERIFY(stats.second.object_counts.at(SkyObject::STAR) >= 1);
        QVERIFY(stats.second.object_counts.at(SkyObject::SUPERNOVA_REMNANT) >= 1);
    }

    void color_strings()
    {
        const std::vector<std::pair<QString, CatalogsDB::CatalogColorMap>> test_data{
            { "#008000", { { "default", "#008000" } } },
            { "#008000;test.colors;#008001",
              { { "default", "#008000" }, { "test.colors", "#008001" } } },
            { "#008000;test.colors;#008001;best.colors;#008002",
              { { "default", "#008000" },
                { "test.colors", "#008001" },
                { "best.colors", "#008002" } } }
        };

        for (const auto &item : test_data)
        {
            QCOMPARE(CatalogsDB::parse_color_string(item.first), item.second);

            QCOMPARE(
                CatalogsDB::parse_color_string(CatalogsDB::to_color_string(item.second)),
                item.second); // the other way around does not have to be invertible
        }

        // more than one theme changes the order in the string
        const auto &simple = test_data.at(1);
        QCOMPARE(CatalogsDB::to_color_string(simple.second), simple.first);

        // Check the behavour when a color specification is missing
        QCOMPARE((CatalogsDB::parse_color_string("#008000;test.colors")),
                 (CatalogsDB::CatalogColorMap{ { "default", "#008000" } }));

        // no default
        QCOMPARE(CatalogsDB::parse_color_string(""), CatalogsDB::CatalogColorMap{});
        QCOMPARE(CatalogsDB::parse_color_string(";test;#008000"),
                 (CatalogsDB::CatalogColorMap{ { "test", "#008000" } }));
    }

    void color_database()
    {
        auto compare_catalog_color_maps = [](CatalogsDB::CatalogColorMap a,
                                             CatalogsDB::CatalogColorMap b) -> void {
            for (auto &item : a)
            {
                QCOMPARE(b[item.first].name(), item.second.name());
            }

            for (auto &item : b)
            {
                QCOMPARE(a[item.first].name(), item.second.name());
            }
        };

        auto compare_color_maps =
            [compare_catalog_color_maps](CatalogsDB::ColorMap a,
                                         CatalogsDB::ColorMap b) -> void {
            for (auto &item : a)
            {
                compare_catalog_color_maps(b[item.first], item.second);
            }

            for (auto &item : b)
            {
                compare_catalog_color_maps(a[item.first], item.second);
            }
        };

        const auto &colors = m_manager.get_catalog_colors();
        compare_color_maps((CatalogsDB::ColorMap{
                               { 1,
                                 {
                                     { "default", "#001000" },
                                     { "test.colors", "#008000" },
                                     { "test1.colors", "#008001" }, // overridden
                                     { "test2.colors", "#001002" }, // from catalog table
                                 } },
                               { 2,
                                 {
                                     { "test1.colors", "#008002" },
                                 } } }),
                           colors);

        compare_catalog_color_maps(m_manager.get_catalog_colors(1), colors.at(1));

        // overwrite default
        auto new_colors            = colors.at(1);
        new_colors["test2.colors"] = "#001003";

        const auto &success = m_manager.insert_catalog_colors(1, new_colors);
        QVERIFY(success.first);

        QCOMPARE(m_manager.get_catalog_colors(1).at("test2.colors"), "#001003");

        auto cat = m_manager.get_catalog(1);
        QVERIFY(cat.first);
        cat.second.color = "#001004";
        QVERIFY(m_manager.update_catalog_meta(cat.second).first);
        QCOMPARE(m_manager.get_catalog_colors(1).at("default"),
                 "#001000"); // does not change
    }
};

QTEST_GUILESS_MAIN(TestCatalogsDB_DBManager);
