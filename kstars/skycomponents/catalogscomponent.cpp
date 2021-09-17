/*
    SPDX-FileCopyrightText: 2001 Jason Harris <jharris@30doradus.org>
    SPDX-FileCopyrightText: 2021 Valentin Boettcher <hiro at protagon.space; @hiro98:tchncs.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <cmath>
#include "catalogscomponent.h"
#include "skypainter.h"
#include "skymap.h"
#include "kstarsdata.h"
#include "Options.h"
#include "MeshIterator.h"
#include "projections/projector.h"
#include "skylabeler.h"
#include "kstars_debug.h"
#include "kstars.h"
#include "skymapcomposite.h"
#include "kspaths.h"
#include "import_skycomp.h"

CatalogsComponent::CatalogsComponent(SkyComposite *parent, const QString &db_filename,
                                     bool load_default)
    : SkyComponent(parent), m_db_manager(db_filename), m_skyMesh{ SkyMesh::Create(
                                                           m_db_manager.htmesh_level()) },
      m_cache(m_skyMesh->size(), calculateCacheSize(Options::dSOCachePercentage()))
{
    if (load_default)
    {
        const auto &default_file = KSPaths::locate(QStandardPaths::AppDataLocation,
                                                   Options::dSODefaultCatalogFilename());

        if (QFile(default_file).exists())
        {
            m_db_manager.import_catalog(default_file, false);
        }
    }

    m_catalog_colors = m_db_manager.get_catalog_colors();
    tryImportSkyComponents();
    qCInfo(KSTARS) << "Loaded DSO catalogs.";
}

double compute_maglim()
{
    double maglim = Options::magLimitDrawDeepSky();

    //adjust maglimit for ZoomLevel
    static const double lgmin{ log10(MINZOOM) };
    static const double lgmax{ log10(MAXZOOM) };
    double lgz = log10(Options::zoomFactor());
    if (lgz <= 0.75 * lgmax)
        maglim -=
            (Options::magLimitDrawDeepSky() - Options::magLimitDrawDeepSkyZoomOut()) *
            (0.75 * lgmax - lgz) / (0.75 * lgmax - lgmin);

    return maglim;
}

void CatalogsComponent::draw(SkyPainter *skyp)
{
    if (!selected() || Options::zoomFactor() < Options::dSOMinZoomFactor())
        return;

    KStarsData *data          = KStarsData::Instance();
    const auto &default_color = data->colorScheme()->colorNamed("DSOColor");
    skyp->setPen(default_color);
    skyp->setBrush(Qt::NoBrush);

    bool showUnknownMagObjects = Options::showUnknownMagObjects();
    auto maglim                = compute_maglim();

    auto &labeler = *SkyLabeler::Instance();
    labeler.setPen(
        QColor(KStarsData::Instance()->colorScheme()->colorNamed("DSNameColor")));
    const auto &color_scheme = KStarsData::Instance()->colorSchemeFileName();

    auto &map       = *SkyMap::Instance();
    auto hideLabels = (map.isSlewing() && Options::hideOnSlew()) ||
                      !(Options::showDeepSkyMagnitudes() || Options::showDeepSkyNames());

    const auto label_padding{ 1 + (1 - (Options::deepSkyLabelDensity() / 100)) * 50 };
    auto &proj = *map.projector();

    updateSkyMesh(map);

    MeshIterator region(m_skyMesh, DRAW_BUF);

    size_t num_trixels{ 0 };

    while (region.hasNext())
    {
        Trixel trixel = region.next();
        num_trixels++;

        auto &objects = m_cache[trixel];
        if (!objects.is_set())
        {
            try
            {
                objects = m_db_manager.get_objects_in_trixel(trixel);
            }
            catch (const CatalogsDB::DatabaseError &e)
            {
                qCCritical(KSTARS)
                    << "Could not load catalog objects in trixel: " << trixel << ", "
                    << e.what();

                KMessageBox::detailedError(
                    nullptr, i18n("Could not load catalog objects in trixel: %1", trixel),
                    e.what());

                throw; // do not silently fail
            }
        }

        for (auto &object : objects.data())
        {
            auto mag          = object.mag();
            bool mag_unknown  = std::isnan(mag) || (mag > 36.0);
            bool magCriterion = (mag_unknown && showUnknownMagObjects) || (mag < maglim);

            if (!magCriterion && !mag_unknown)
                break; // the objects are strictly sorted by magnitude
                       // unknown magnitude first
            if (!magCriterion)
                continue;

            double size = object.a() * dms::PI * Options::zoomFactor() / 10800.0;

            bool sizeCriterion =
                (size > 1.0 || size == 0 || Options::zoomFactor() > 2000.);

            if (sizeCriterion)
            {
                object.JITupdate();
                auto &color = m_catalog_colors[object.catalogId()][color_scheme];
                if (!color.isValid())
                {
                    color = m_catalog_colors[object.catalogId()]["default"];

                    if (!color.isValid())
                    {
                        color = default_color;
                    }
                }

                skyp->setPen(color);

                if (Options::showInlineImages())
                    object.load_image();

                if (skyp->drawCatalogObject(object) && !hideLabels)
                {
                    labeler.drawNameLabel(&object, proj.toScreen(&object), label_padding);
                }
            }
        }
    }

    // prune only if the to-be-pruned trixels are likely not visible
    // and we are not zooming
    m_cache.prune(num_trixels * 1.2);
};

void CatalogsComponent::updateSkyMesh(SkyMap &map, MeshBufNum_t buf)
{
    SkyPoint *focus = map.focus();
    float radius    = map.projector()->fov();
    if (radius > 180.0)
        radius = 180.0;

    m_skyMesh->aperture(focus, radius + 1.0, buf);
}

CatalogObject &CatalogsComponent::insertStaticObject(const CatalogObject &obj)
{
    auto trixel     = m_skyMesh->index(&obj);
    auto &lst       = m_static_objects[trixel];
    auto found_iter = std::find(lst.begin(), lst.end(), obj);

    // Ideally, we would remove the object from ObjectsList if it's
    // respective catalog is disabled, but there ain't a good way to
    // do this right now

    if (!(found_iter == lst.end()))
    {
        auto &found = *found_iter;
        found       = obj;
        found.JITupdate();
        return found;
    }

    auto copy = obj;
    copy.JITupdate();

    lst.push_back(std::move(copy));
    auto &inserted = lst.back();

    // we don't bother with translations here
    auto &object_list = objectLists(inserted.type());

    object_list.append({ inserted.name(), &inserted });
    if (inserted.longname() != inserted.name())
        object_list.append({ inserted.longname(), &inserted });

    return inserted;
}

SkyObject *CatalogsComponent::findByName(const QString &name)
{
    auto objects = m_db_manager.find_objects_by_name(name, 1, true);

    if (objects.size() == 0)
        return nullptr;

    return &insertStaticObject(objects.front());
}

void CatalogsComponent::objectsInArea(QList<SkyObject *> &list, const SkyRegion &region)
{
    if (!selected())
        return;

    for (SkyRegion::const_iterator it = region.constBegin(); it != region.constEnd();
         ++it)
    {
        try
        {
            for (auto &dso : m_db_manager.get_objects_in_trixel(it.key()))
            {
                auto &obj = insertStaticObject(dso);
                list.append(&obj);
            }
        }
        catch (const CatalogsDB::DatabaseError &e)
        {
            qCCritical(KSTARS) << "Could not load catalog objects in trixel: " << it.key()
                               << ", " << e.what();

            KMessageBox::detailedError(
                nullptr, i18n("Could not load catalog objects in trixel: %1", it.key()),
                e.what());
            throw; // do not silently fail
        }
    }
}

SkyObject *CatalogsComponent::objectNearest(SkyPoint *p, double &maxrad)
{
    if (!selected())
        return nullptr;

    m_skyMesh->aperture(p, maxrad, OBJ_NEAREST_BUF);
    MeshIterator region(m_skyMesh, OBJ_NEAREST_BUF);
    double smallest_r{ 360 };
    CatalogObject nearest{};
    bool found{ false };

    while (region.hasNext())
    {
        auto trixel = region.next();
        try
        {
            auto objects = m_db_manager.get_objects_in_trixel(trixel);
            if (!found)
                found = objects.size() > 0;

            for (auto &obj : objects)
            {
                obj.JITupdate();

                double r = obj.angularDistanceTo(p).Degrees();
                if (r < smallest_r)
                {
                    smallest_r = r;
                    nearest    = obj;
                }
            }
        }
        catch (const CatalogsDB::DatabaseError &e)
        {
            KMessageBox::detailedError(
                nullptr, i18n("Could not load catalog objects in trixel: %1", trixel),
                e.what());
            throw; // do not silently fail
        }
    }

    if (!found)
        return nullptr;

    maxrad = smallest_r;

    return &insertStaticObject(nearest);
}

void CatalogsComponent::tryImportSkyComponents()
{
    auto skycom_db = SkyComponentsImport::get_skycomp_db();
    if (!skycom_db.first)
        return;

    const auto move_skycompdb = [&]()
    {
        const auto &path = skycom_db.second.databaseName();
        const auto &new_path =
            QString("%1.%2.backup").arg(path).arg(QDateTime::currentMSecsSinceEpoch());

        QFile::rename(path, new_path);
    };

    const auto resp = KMessageBox::questionYesNoCancel(
        nullptr, i18n("Import custom and internet resolved objects "
                      "from the old DSO database into the new one?"));

    if (resp != KMessageBox::Yes)
    {
        move_skycompdb();
        return;
    }

    const auto &success = SkyComponentsImport::get_objects(skycom_db.second);
    if (!std::get<0>(success))
        KMessageBox::detailedError(nullptr, i18n("Could not import the objects."),
                                   std::get<1>(success));

    const auto &add_success =
        m_db_manager.add_objects(CatalogsDB::user_catalog_id, std::get<2>(success));

    if (!add_success.first)
        KMessageBox::detailedError(nullptr, i18n("Could not import the objects."),
                                   add_success.second);
    else
    {
        KMessageBox::information(
            nullptr, i18np("Successfully added %1 object to the user catalog.",
                           "Successfully added %1 objects to the user catalog.",
                           std::get<2>(success).size()));
        move_skycompdb();
    }
};
