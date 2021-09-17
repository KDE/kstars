/*
    SPDX-FileCopyrightText: 2001 Jason Harris <jharris@30doradus.org>
    SPDX-FileCopyrightText: 2021 Valentin Boettcher <hiro at protagon.space; @hiro98:tchncs.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "skycomponent.h"
#include "catalogsdb.h"
#include "catalogobject.h"
#include "skymesh.h"
#include "trixelcache.h"
#include "Options.h"
#include <unordered_map>

class SkyMesh;
class SkyMap;

/**
 * \brief Represents objects loaded from an sqlite backed, trixel
 * indexed catalog.
 *
 * The component doesn't follow the traditional list approach and
 * loads it's skyobjects into an LRU cache (`TrixelCache`). For
 * puproses of compatiblility with object search etc. some of the
 * brightest objects are loaded into `m_static_objects` and registered
 * within the component system. Furthermore, if some part of the code
 * demands a pointer to a CatalogObject, it will be allocated into
 * `m_static_objects` on demand.
 *
 * If you want to access DSOs in _new_ code you should use a local
 * instance of `CatalogsDB::DBManager` instead and call `dropCache` if
 * necessary.
 *
 * \sa CatalogsDB::DBManager
 */
class CatalogsComponent : public SkyComponent
{
    public:
        using ObjectList = std::vector<CatalogObject>;

        /**
         * Constructs the Catalogscomponent with a \p parent and a
         * database file under the path \p db_filename. If \p load_ngc is
         * specified, an attempt is made to load the default catalog from
         * the default location into the db.
         *
         * The lru cache for the objects will be initialized to a capacity
         * configurable by Options::dSOCachePercentage.
         */
        explicit CatalogsComponent(SkyComposite *parent, const QString &db_filename,
                                   bool load_default = false);

        ~CatalogsComponent() override = default;

        /**
         * Draws the objects in the currently visible trixels by
         * dynamically loading them from the database.
         */
        void draw(SkyPainter *skyp) override;

        /**
         * Set the cache size to the new \p percentage.
         *
         * The cache stores the objects of a certain \p percentage of all
         * trixels. Setting `percentage = 100` short circuits the cache and loads
         * all the objects into memory. This is reasonable for catalog sizes up
         * to `10_000` objects.
         */
        void resizeCache(const int percentage)
        {
            m_cache.set_size(calculateCacheSize(percentage));
        };

        /**
         * \short Search the underlying database for an object with the \p
         * name. \sa `CatalogsDB::DBManager::find_object_by_name` for
         * details.
         *
         * If multiple objects match, the one with the hightest magnitude is
         * returned.
         *
         * \return a pointer to the SkyObject whose name matches the argument, or
         * a nullptr pointer if no match was found. (Due to way KStars works)
         */
        SkyObject *findByName(const QString &name) override;

        void objectsInArea(QList<SkyObject *> &list, const SkyRegion &region) override;

        SkyObject *objectNearest(SkyPoint *p, double &maxrad) override;

        /**
         * Insert an object \p obj into `m_static_objects` and return a
         * reference to the newly inserted object. If the object is
         * already present in the list, return a reference to
         * that. Furthermore the object will be updated
         * (`CatalogObject::JITupdate`) and inserted into the parent's
         * `objectLists`.
         */
        CatalogObject &insertStaticObject(const CatalogObject &obj);

        /**
         * Clear the internal cache and effectively reload all objects
         * from the database.
         */
        void dropCache()
        {
            m_cache.clear();
            m_catalog_colors = m_db_manager.get_catalog_colors();
        };

        /**
         * Wether to show the DSOs.
         */
        bool selected() override
        {
            return Options::showDeepSky();
        };

    private:
        /**
         * The database interface for the catalog.
         */
        CatalogsDB::DBManager m_db_manager;

        /**
         * A pointer to a SkyMesh of the appropriate level.
         *
         * @note The use of a pointer here is a legacy from the
         * SkyComponent implementation.
         */
        SkyMesh *m_skyMesh;

        /**
         * The main container for the currently loaded objects.
         */
        ObjectList m_objects;

        /**
         * The cache holding the DSOs
         */
        TrixelCache<ObjectList> m_cache;

        /**
         * A trixel indexed map of lists containing manually loaded
         * `CatalogObject`s.
         *
         * Because some `KStars` internal code requires pointers to SkyObjects
         * and this component doesn't hold its objects, we have create a space to
         * to own the objets that we create in methods like `findByName`. Thus it
         * is expected that this list won't hold many objects and doesn't have to
         * be emptied at runtime. The objects in this map are not drawn and have
         * to be updated manually.
         *
         * __No objects should ever be removed from this list, as references and
         * pointers to list members are required to remain valid.__
         *
         * __In new code, a local instance of `CatalogsDB::DBManager` should be
         * used when access to CatalogObjects is required. Call `dropCache` if
         * required.__
         */
        std::unordered_map<Trixel, CatalogsDB::CatalogObjectList> m_static_objects;

        /**
         * A cache for catalog colors.
         */
        CatalogsDB::ColorMap m_catalog_colors;

        //@{
        /** Helpers */

        void updateSkyMesh(SkyMap &map, MeshBufNum_t buf = DRAW_BUF);
        size_t calculateCacheSize(const unsigned int percentage)
        {
            return m_skyMesh->size() * percentage / 100;
        }

        /**
         * Try importing the old skycomponents database.
         */
        void tryImportSkyComponents();

        //@}
};
